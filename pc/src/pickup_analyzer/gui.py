from __future__ import annotations

import json
import queue
import threading
import tkinter as tk
from collections import deque
from dataclasses import dataclass
from datetime import datetime
from tkinter import filedialog, messagebox, ttk

from .client import AnalyzerClient
from .fitting import RlcFit, fit_rlc
from .plot import SweepPlot, components_series, magnitude_series, nyquist_series, phase_series
from .sweep import Sweep, SweepPoint, load_sweeps, logarithmic_frequencies, save_sweeps
from .transport import SerialTransport, TcpTransport, Transport


@dataclass
class SweepEntry:
    sweep: Sweep
    visible: bool = True
    fit: RlcFit | None = None
    show_fit: bool = False


class AnalyzerGui:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("Guitar Pickup Impedance Analyzer")
        root.geometry("1050x780")
        root.minsize(950, 700)
        self.transport: Transport | None = None
        self.client: AnalyzerClient | None = None
        self.events: queue.Queue[tuple[str, object]] = queue.Queue()
        self.progress_lock = threading.Lock()
        self.latest_progress: tuple[int, int, list[SweepPoint], bool] | None = None
        self.latest_complete: Sweep | None = None
        self.console_lock = threading.Lock()
        self.pending_console: deque[str] = deque(maxlen=4000)
        self.console_line_count = 0
        self.stop_requested = threading.Event()
        self.worker: threading.Thread | None = None
        self.current_sweep: Sweep | None = None
        self.sweep_entries: list[SweepEntry] = []
        self.active_entry: SweepEntry | None = None
        controls = ttk.Frame(root, padding=8)
        controls.pack(fill="x")
        self._connection_controls(controls)
        self._sweep_controls(controls)
        self._file_controls(controls)
        self._magnitude_axis_controls(root)
        self._sweep_manager(root)
        notebook = ttk.Notebook(root)
        notebook.pack(fill="both", expand=True, padx=8, pady=(0, 8))
        self.plots = [
            self._plot(notebook, "Magnitude", "Magnitude", "Frequency (Hz)", "|Z| (ohm, log)", magnitude_series, True, True),
            self._plot(notebook, "Phase", "Phase", "Frequency (Hz)", "Phase (degrees)", phase_series,
                       True, y_range=(-180.0, 180.0)),
            self._plot(notebook, "Real / Imag", "Components", "Frequency (Hz)", "Impedance (ohm)", components_series, True),
            self._plot(notebook, "Nyquist", "Nyquist", "Real Z (ohm)", "Imaginary Z (ohm)",
                       nyquist_series, False, equal_units=True),
        ]
        self.magnitude_plot = self.plots[0]
        self._console_tab(notebook)
        self.status = tk.StringVar(value="Start the virtual target, then connect.")
        ttk.Label(root, textvariable=self.status, padding=(10, 3)).pack(fill="x")
        root.protocol("WM_DELETE_WINDOW", self.close)
        root.after(50, self.poll_events)

    def _connection_controls(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(parent, text="Connection", padding=8)
        frame.pack(side="left", fill="y", padx=(0, 6))
        self.mode = tk.StringVar(value="TCP")
        ttk.Combobox(frame, textvariable=self.mode, values=("TCP", "Serial"), width=7,
                     state="readonly").grid(row=0, column=0)
        self.endpoint = tk.StringVar(value="127.0.0.1:8765")
        ttk.Entry(frame, textvariable=self.endpoint, width=20).grid(row=0, column=1, padx=5)
        self.connect_button = ttk.Button(frame, text="Connect", command=self.connect)
        self.connect_button.grid(row=0, column=2)
        self.device_info_text = tk.StringVar(value="Not connected")
        ttk.Label(frame, textvariable=self.device_info_text, justify="left").grid(
            row=1, column=0, columnspan=3, sticky="w", pady=(6, 0))

    def _sweep_controls(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(parent, text="Log sweep", padding=8)
        frame.pack(side="left", fill="y", padx=6)
        self.start_hz, self.stop_hz, self.point_count = tk.DoubleVar(value=20), tk.DoubleVar(value=20000), tk.IntVar(value=101)
        for column, (label, variable, width) in enumerate((("Start Hz", self.start_hz, 8),
                                                           ("Stop Hz", self.stop_hz, 8),
                                                           ("Points", self.point_count, 6))):
            ttk.Label(frame, text=label).grid(row=0, column=column)
            ttk.Entry(frame, textvariable=variable, width=width).grid(row=1, column=column, padx=3)
        self.once_button = ttk.Button(frame, text="Run once", command=lambda: self.start_sweep(False))
        self.once_button.grid(row=2, column=0, pady=(6, 0))
        self.continuous_button = ttk.Button(frame, text="Continuous", command=lambda: self.start_sweep(True))
        self.continuous_button.grid(row=2, column=1, pady=(6, 0))
        self.stop_button = ttk.Button(frame, text="Stop", command=self.stop_sweep, state="disabled")
        self.stop_button.grid(row=2, column=2, pady=(6, 0))

    def _file_controls(self, parent: ttk.Frame) -> None:
        frame = ttk.LabelFrame(parent, text="Sweeps", padding=8)
        frame.pack(side="left", fill="y", padx=(6, 0))
        ttk.Button(frame, text="Save all…", command=self.save).grid(row=0, column=0, padx=2)
        ttk.Button(frame, text="Load…", command=self.load).grid(row=0, column=1, padx=2)

    def _magnitude_axis_controls(self, parent) -> None:
        frame = ttk.Frame(parent, padding=(10, 0, 10, 6))
        frame.pack(fill="x")
        ttk.Label(frame, text="Magnitude axis:").pack(side="left")
        self.magnitude_auto = tk.BooleanVar(value=True)
        ttk.Checkbutton(frame, text="Auto", variable=self.magnitude_auto,
                        command=self.apply_magnitude_axis).pack(side="left", padx=(6, 12))
        ttk.Label(frame, text="Minimum Ω").pack(side="left")
        self.magnitude_min = tk.DoubleVar(value=1000.0)
        ttk.Entry(frame, textvariable=self.magnitude_min, width=10).pack(side="left", padx=(4, 10))
        ttk.Label(frame, text="Maximum Ω").pack(side="left")
        self.magnitude_max = tk.DoubleVar(value=1000000.0)
        ttk.Entry(frame, textvariable=self.magnitude_max, width=10).pack(side="left", padx=4)
        ttk.Button(frame, text="Apply", command=self.apply_magnitude_axis).pack(side="left", padx=6)

    def _sweep_manager(self, parent) -> None:
        frame = ttk.LabelFrame(parent, text="Loaded sweeps", padding=6)
        frame.pack(fill="x", padx=8, pady=(0, 6))
        table_frame = ttk.Frame(frame)
        table_frame.pack(side="left", fill="x", expand=True)
        columns = ("visible", "fit", "resistance", "inductance", "capacitance", "r2", "sigma")
        self.sweep_table = ttk.Treeview(table_frame, columns=columns, show="tree headings", height=5)
        self.sweep_table.heading("#0", text="Name")
        headings = {"visible": "Sweep", "fit": "Fit", "resistance": "R (Ω)",
                    "inductance": "L (H)", "capacitance": "C (pF)",
                    "r2": "R²", "sigma": "σ (Ω)"}
        for column, heading in headings.items():
            self.sweep_table.heading(column, text=heading)
        self.sweep_table.column("#0", width=250, stretch=True)
        self.sweep_table.column("visible", width=55, anchor="center", stretch=False)
        self.sweep_table.column("fit", width=45, anchor="center", stretch=False)
        for column in ("resistance", "inductance", "capacitance", "r2", "sigma"):
            self.sweep_table.column(column, width=92, anchor="e", stretch=False)
        scrollbar = ttk.Scrollbar(table_frame, orient="horizontal", command=self.sweep_table.xview)
        self.sweep_table.configure(xscrollcommand=scrollbar.set)
        self.sweep_table.pack(fill="x", expand=True)
        scrollbar.pack(fill="x")
        self.sweep_table.bind("<Button-1>", self._sweep_table_click)
        buttons = ttk.Frame(frame)
        buttons.pack(side="left", padx=(8, 0), anchor="n")
        ttk.Button(buttons, text="Delete", command=self.delete_selected).pack(fill="x", pady=3)

    def _console_tab(self, notebook: ttk.Notebook) -> None:
        frame = ttk.Frame(notebook, padding=6)
        notebook.add(frame, text="Console")
        toolbar = ttk.Frame(frame)
        toolbar.pack(fill="x", pady=(0, 5))
        ttk.Label(toolbar, text="Newline-delimited JSON traffic (newest 2,000 lines)").pack(side="left")
        ttk.Button(toolbar, text="Clear", command=self.clear_console).pack(side="right")
        container = ttk.Frame(frame)
        container.pack(fill="both", expand=True)
        self.console = tk.Text(container, wrap="none", background="#15181d", foreground="#d7dce2",
                               insertbackground="#d7dce2", font=("TkFixedFont", 9), state="disabled")
        vertical = ttk.Scrollbar(container, orient="vertical", command=self.console.yview)
        horizontal = ttk.Scrollbar(container, orient="horizontal", command=self.console.xview)
        self.console.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
        self.console.grid(row=0, column=0, sticky="nsew")
        vertical.grid(row=0, column=1, sticky="ns")
        horizontal.grid(row=1, column=0, sticky="ew")
        container.rowconfigure(0, weight=1)
        container.columnconfigure(0, weight=1)

    def clear_console(self) -> None:
        with self.console_lock:
            self.pending_console.clear()
        self.console.configure(state="normal")
        self.console.delete("1.0", "end")
        self.console.configure(state="disabled")
        self.console_line_count = 0

    def _log_message(self, direction: str, message: dict) -> None:
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        line = f"{timestamp} {direction} {json.dumps(message, separators=(',', ':'))}\n"
        with self.console_lock:
            self.pending_console.append(line)

    @staticmethod
    def _plot(notebook, tab_name, title, x_label, y_label, builder, log_x, log_y=False,
              y_range=None, equal_units=False):
        frame = ttk.Frame(notebook)
        notebook.add(frame, text=tab_name)
        plot = SweepPlot(frame, title, x_label, y_label, builder, log_x, log_y,
                         y_range, equal_units)
        plot.pack(fill="both", expand=True)
        return plot

    def apply_magnitude_axis(self) -> None:
        if not hasattr(self, "magnitude_plot"):
            return
        if self.magnitude_auto.get():
            self.magnitude_plot.set_y_range(None)
            return
        try:
            minimum, maximum = self.magnitude_min.get(), self.magnitude_max.get()
            if minimum <= 0 or maximum <= minimum:
                raise ValueError("limits must be positive, with maximum greater than minimum")
            self.magnitude_plot.set_y_range((minimum, maximum))
        except (ValueError, tk.TclError) as error:
            messagebox.showerror("Invalid magnitude axis", str(error))

    def connect(self) -> None:
        if self.worker and self.worker.is_alive():
            messagebox.showinfo("Sweep active", "Stop the current sweep before reconnecting.")
            return
        try:
            if self.transport:
                self.client.close()
            if self.mode.get() == "TCP":
                host, port = self.endpoint.get().rsplit(":", 1)
                self.transport = TcpTransport(host, int(port))
            else:
                self.transport = SerialTransport(self.endpoint.get())
            self.client = AnalyzerClient(self.transport, self._log_message)
            info = self.client.device_info()
            capabilities = ", ".join(info.get("capabilities", []))
            self.device_info_text.set(
                f"Target: {info.get('target_name', 'unknown')}\n"
                f"Application: {info.get('application_name', 'unknown')} "
                f"v{info.get('application_version', 'unknown')}\n"
                f"Protocol: v{info.get('protocol_version', 'unknown')}\n"
                f"Capabilities: {capabilities or 'not reported'}")
            self.status.set(f"Connected via {self.mode.get()} to {self.endpoint.get()}")
            self.connect_button.configure(text="Reconnect")
        except Exception as error:
            self.transport = None
            self.client = None
            self.device_info_text.set("Not connected")
            messagebox.showerror("Connection failed", str(error))

    def start_sweep(self, continuous: bool) -> None:
        if not self.client:
            messagebox.showinfo("Not connected", "Connect to a target first.")
            return
        if self.worker and self.worker.is_alive():
            return
        try:
            frequencies = logarithmic_frequencies(self.start_hz.get(), self.stop_hz.get(), self.point_count.get())
        except (ValueError, tk.TclError) as error:
            messagebox.showerror("Invalid sweep", str(error))
            return
        self.stop_requested.clear()
        self.current_sweep = Sweep("Live acquisition", [])
        self.active_entry = SweepEntry(self.current_sweep)
        self.sweep_entries.append(self.active_entry)
        self._refresh_sweep_table(select=self.active_entry)
        self._set_running(True)
        self.worker = threading.Thread(target=self._sweep_worker, args=(frequencies, continuous), daemon=True)
        self.worker.start()

    def _sweep_worker(self, frequencies: list[float], continuous: bool) -> None:
        try:
            while not self.stop_requested.is_set():
                points: list[SweepPoint] = []
                self.client.start_sweep(frequencies[0], frequencies[-1], len(frequencies))
                while True:
                    if self.stop_requested.is_set():
                        self.client.stop_sweep()
                        break
                    event = self.client.next_event(timeout=10.0)
                    if event.get("object") == "sweep" and event.get("action") == "complete":
                        break
                    if event.get("object") != "measurement":
                        continue
                    data = event["data"]
                    point = SweepPoint(float(data["f"]), float(data["z"]["re"]), float(data["z"]["im"]))
                    index = len(points)
                    # The UI consumes only the newest snapshot at its own frame rate.
                    # This prevents thousands of redraw events from accumulating.
                    with self.progress_lock:
                        points.append(point)
                        self.latest_progress = (index, len(frequencies), points, continuous)
                if len(points) == len(frequencies):
                    with self.progress_lock:
                        self.latest_progress = None
                        # Completion is state, not an event: a fast simulator may
                        # finish many passes between GUI frames. Only the newest
                        # complete pass is useful as the scope back buffer.
                        self.latest_complete = Sweep(
                            datetime.now().strftime("Sweep %Y-%m-%d %H:%M:%S"), points)
                if not continuous:
                    break
        except Exception as error:
            self.events.put(("error", error))
        finally:
            self.events.put(("stopped", None))

    def stop_sweep(self) -> None:
        self.stop_requested.set()
        self.status.set("Stopping after the current measurement…")

    def _set_running(self, running: bool) -> None:
        state = "disabled" if running else "normal"
        for button in (self.once_button, self.continuous_button, self.connect_button):
            button.configure(state=state)
        self.stop_button.configure(state="normal" if running else "disabled")

    def poll_events(self) -> None:
        try:
            while True:
                event, value = self.events.get_nowait()
                if event == "error":
                    messagebox.showerror("Sweep failed", str(value))
                elif event == "stopped":
                    self._set_running(False)
        except queue.Empty:
            pass
        with self.progress_lock:
            complete = self.latest_complete
            self.latest_complete = None
            if self.latest_progress is None:
                progress = None
            else:
                index, total, measured, continuous = self.latest_progress
                progress = (index, total, measured.copy(), continuous)
        needs_redraw = False
        if complete is not None:
            self.current_sweep = complete
            if self.active_entry is not None:
                self.active_entry.sweep = complete
                self._fit_entry(self.active_entry)
            self.status.set(f"Completed {complete.name}: {len(complete.points)} points")
            self._refresh_sweep_table(select=self.active_entry)
            needs_redraw = True
        if progress is not None:
            index, total, measured, continuous = progress
            if continuous and self.current_sweep and len(self.current_sweep.points) == total:
                live_points = self.current_sweep.points.copy()
                live_points[:len(measured)] = measured
            else:
                live_points = measured
            self.current_sweep = Sweep("Live", live_points)
            if self.active_entry is not None:
                self.active_entry.sweep = self.current_sweep
            self.status.set(f"Measuring point {index + 1} of {total}")
            needs_redraw = True
        if needs_redraw:
            self.refresh_plots()
        self._flush_console()
        self.root.after(50, self.poll_events)

    def _flush_console(self) -> None:
        with self.console_lock:
            if not self.pending_console:
                return
            lines = list(self.pending_console)
            self.pending_console.clear()
        at_bottom = self.console.yview()[1] >= 0.999
        self.console.configure(state="normal")
        self.console.insert("end", "".join(lines))
        self.console_line_count += len(lines)
        excess = self.console_line_count - 2000
        if excess > 0:
            self.console.delete("1.0", f"{excess + 1}.0")
            self.console_line_count -= excess
        if at_bottom:
            self.console.see("end")
        self.console.configure(state="disabled")

    def refresh_plots(self) -> None:
        sweeps: list[Sweep] = []
        for entry in self.sweep_entries:
            if entry.visible:
                sweeps.append(entry.sweep)
            if entry.show_fit and entry.fit is not None:
                sweeps.append(entry.fit.as_sweep(entry.sweep))
        for plot in self.plots:
            plot.set_sweeps(sweeps)

    def _selected_entry(self) -> SweepEntry | None:
        selection = self.sweep_table.selection()
        if not selection:
            messagebox.showinfo("No selection", "Select a sweep from the list first.")
            return None
        index = int(selection[0])
        return self.sweep_entries[index] if index < len(self.sweep_entries) else None

    def _refresh_sweep_table(self, select: SweepEntry | None = None) -> None:
        selected = select or self._selected_entry_quiet()
        self.sweep_table.delete(*self.sweep_table.get_children())
        selected_id = None
        for index, entry in enumerate(self.sweep_entries):
            item_id = str(index)
            fit = entry.fit
            fit_values = ((f"{fit.resistance_ohm:.6g}", f"{fit.inductance_h:.6g}",
                           f"{fit.capacitance_f * 1e12:.6g}", f"{fit.r_squared:.6f}",
                           f"{fit.standard_deviation_ohm:.6g}") if fit else ("—",) * 5)
            self.sweep_table.insert("", "end", iid=item_id, text=entry.sweep.name,
                                    values=("☑" if entry.visible else "☐",
                                            "☑" if entry.show_fit else "☐", *fit_values))
            if entry is selected:
                selected_id = item_id
        if selected_id is not None:
            self.sweep_table.selection_set(selected_id)

    def _selected_entry_quiet(self) -> SweepEntry | None:
        selection = self.sweep_table.selection()
        if not selection:
            return None
        index = int(selection[0])
        return self.sweep_entries[index] if index < len(self.sweep_entries) else None

    def _sweep_table_click(self, event) -> str | None:
        row = self.sweep_table.identify_row(event.y)
        column = self.sweep_table.identify_column(event.x)
        if not row:
            return None
        self.sweep_table.selection_set(row)
        entry = self.sweep_entries[int(row)]
        if column == "#1":
            entry.visible = not entry.visible
        elif column == "#2" and entry.fit is not None:
            entry.show_fit = not entry.show_fit
        else:
            return None
        self._refresh_sweep_table(select=entry)
        self.refresh_plots()
        return "break"

    def delete_selected(self) -> None:
        entry = self._selected_entry()
        if not entry:
            return
        if entry is self.active_entry and self.worker and self.worker.is_alive():
            messagebox.showinfo("Sweep active", "Stop this sweep before deleting it.")
            return
        self.sweep_entries.remove(entry)
        if entry is self.active_entry:
            self.active_entry = None
            self.current_sweep = None
        self._refresh_sweep_table()
        self.refresh_plots()

    @staticmethod
    def _fit_entry(entry: SweepEntry) -> None:
        try:
            entry.fit = fit_rlc(entry.sweep)
        except ValueError:
            entry.fit = None
            entry.show_fit = False

    def save(self) -> None:
        sweeps = [entry.sweep for entry in self.sweep_entries]
        if not sweeps:
            messagebox.showinfo("No sweeps", "There are no sweeps to save.")
            return
        path = filedialog.asksaveasfilename(defaultextension=".json",
                                            filetypes=(("Sweep JSON", "*.json"), ("All files", "*")))
        if path:
            try:
                save_sweeps(path, sweeps)
                self.status.set(f"Saved {len(sweeps)} sweep(s) to {path}")
            except Exception as error:
                messagebox.showerror("Save failed", str(error))

    def load(self) -> None:
        path = filedialog.askopenfilename(filetypes=(("Sweep JSON", "*.json"), ("All files", "*")))
        if path:
            try:
                loaded = load_sweeps(path)
                entries = [SweepEntry(sweep) for sweep in loaded]
                for entry in entries:
                    self._fit_entry(entry)
                self.sweep_entries.extend(entries)
                self.current_sweep = loaded[-1]
                self.active_entry = entries[-1]
                self.status.set(f"Loaded {len(loaded)} sweep(s) from {path}")
                self._refresh_sweep_table(select=entries[-1])
                self.refresh_plots()
            except Exception as error:
                messagebox.showerror("Load failed", str(error))

    def close(self) -> None:
        self.stop_requested.set()
        if self.client:
            self.client.close()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    AnalyzerGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
