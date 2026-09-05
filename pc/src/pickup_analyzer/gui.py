from __future__ import annotations

import json
import queue
import threading
import tkinter as tk
from collections import deque
from datetime import datetime
from tkinter import filedialog, messagebox, ttk

from .plot import SweepPlot, components_series, magnitude_series, nyquist_series, phase_series
from .sweep import Sweep, SweepPoint, load_sweeps, logarithmic_frequencies, save_sweeps
from .transport import SerialTransport, TcpTransport, Transport


class AnalyzerGui:
    def __init__(self, root: tk.Tk):
        self.root = root
        root.title("Guitar Pickup Impedance Analyzer")
        root.geometry("1050x780")
        root.minsize(950, 700)
        self.transport: Transport | None = None
        self.transaction_id = 0
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
        self.overlays: list[Sweep] = []
        controls = ttk.Frame(root, padding=8)
        controls.pack(fill="x")
        self._connection_controls(controls)
        self._sweep_controls(controls)
        self._file_controls(controls)
        self._magnitude_axis_controls(root)
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
        ttk.Button(frame, text="Keep overlay", command=self.keep_overlay).grid(row=0, column=0, padx=2)
        ttk.Button(frame, text="Clear overlays", command=self.clear_overlays).grid(row=0, column=1, padx=2)
        ttk.Button(frame, text="Save…", command=self.save).grid(row=1, column=0, pady=(6, 0))
        ttk.Button(frame, text="Load…", command=self.load).grid(row=1, column=1, pady=(6, 0))

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
                self.transport.close()
            if self.mode.get() == "TCP":
                host, port = self.endpoint.get().rsplit(":", 1)
                self.transport = TcpTransport(host, int(port))
            else:
                self.transport = SerialTransport(self.endpoint.get())
            self.status.set(f"Connected via {self.mode.get()} to {self.endpoint.get()}")
            self.connect_button.configure(text="Reconnect")
        except Exception as error:
            self.transport = None
            messagebox.showerror("Connection failed", str(error))

    def start_sweep(self, continuous: bool) -> None:
        if not self.transport:
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
        self._set_running(True)
        self.worker = threading.Thread(target=self._sweep_worker, args=(frequencies, continuous), daemon=True)
        self.worker.start()

    def _sweep_worker(self, frequencies: list[float], continuous: bool) -> None:
        try:
            while not self.stop_requested.is_set():
                points: list[SweepPoint] = []
                for index, frequency in enumerate(frequencies):
                    if self.stop_requested.is_set():
                        break
                    self.transaction_id += 1
                    request = {"type": "measure_impedance", "direction": "request",
                               "transaction_id": self.transaction_id,
                               "payload": {"frequency_hz": frequency}}
                    self._log_message("TX", request)
                    response = self.transport.transact(request)
                    self._log_message("RX", response)
                    if response.get("type") == "error":
                        raise RuntimeError(response.get("payload", {}).get("message", "target error"))
                    payload = response["payload"]
                    point = SweepPoint(float(payload["frequency_hz"]), float(payload["real_ohm"]),
                                       float(payload["imaginary_ohm"]))
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
            self.status.set(f"Completed {complete.name}: {len(complete.points)} points")
            needs_redraw = True
        if progress is not None:
            index, total, measured, continuous = progress
            if continuous and self.current_sweep and len(self.current_sweep.points) == total:
                live_points = self.current_sweep.points.copy()
                live_points[:len(measured)] = measured
            else:
                live_points = measured
            self.current_sweep = Sweep("Live", live_points)
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
        sweeps = self.overlays + ([self.current_sweep] if self.current_sweep else [])
        for plot in self.plots:
            plot.set_sweeps(sweeps)

    def keep_overlay(self) -> None:
        if not self.current_sweep or not self.current_sweep.points:
            messagebox.showinfo("No sweep", "Run or load a sweep first.")
            return
        self.overlays.append(Sweep(self.current_sweep.name, self.current_sweep.points.copy()))
        self.status.set(f"Kept {self.current_sweep.name} as overlay")
        self.refresh_plots()

    def clear_overlays(self) -> None:
        self.overlays.clear()
        self.refresh_plots()

    def save(self) -> None:
        sweeps = self.overlays + ([self.current_sweep] if self.current_sweep else [])
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
                self.overlays.extend(loaded[:-1])
                self.current_sweep = loaded[-1]
                self.status.set(f"Loaded {len(loaded)} sweep(s) from {path}")
                self.refresh_plots()
            except Exception as error:
                messagebox.showerror("Load failed", str(error))

    def close(self) -> None:
        self.stop_requested.set()
        if self.transport:
            self.transport.close()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    AnalyzerGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()
