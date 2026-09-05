from __future__ import annotations

import cmath
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk

from .transport import SerialTransport, TcpTransport, Transport


class AnalyzerGui:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Guitar Pickup Impedance Analyzer")
        self.transport: Transport | None = None
        self.transaction_id = 0
        self.events: queue.Queue[tuple[bool, object]] = queue.Queue()

        connection = ttk.LabelFrame(root, text="Connection", padding=10)
        connection.pack(fill="x", padx=10, pady=10)
        self.mode = tk.StringVar(value="TCP")
        ttk.Combobox(connection, textvariable=self.mode, values=("TCP", "Serial"),
                     width=8, state="readonly").grid(row=0, column=0)
        self.endpoint = tk.StringVar(value="127.0.0.1:8765")
        ttk.Entry(connection, textvariable=self.endpoint, width=28).grid(row=0, column=1, padx=8)
        self.connect_button = ttk.Button(connection, text="Connect", command=self.connect)
        self.connect_button.grid(row=0, column=2)

        measurement = ttk.LabelFrame(root, text="Single measurement", padding=10)
        measurement.pack(fill="x", padx=10, pady=(0, 10))
        ttk.Label(measurement, text="Frequency (Hz)").grid(row=0, column=0)
        self.frequency = tk.DoubleVar(value=1000.0)
        ttk.Entry(measurement, textvariable=self.frequency, width=14).grid(row=0, column=1, padx=8)
        ttk.Button(measurement, text="Measure", command=self.measure).grid(row=0, column=2)

        self.result = tk.StringVar(value="Connect to a target to begin.")
        ttk.Label(root, textvariable=self.result, padding=10, justify="left").pack(fill="x")
        self.root.protocol("WM_DELETE_WINDOW", self.close)
        self.root.after(50, self.poll_events)

    def connect(self) -> None:
        try:
            if self.transport:
                self.transport.close()
            if self.mode.get() == "TCP":
                host, port = self.endpoint.get().rsplit(":", 1)
                self.transport = TcpTransport(host, int(port))
            else:
                self.transport = SerialTransport(self.endpoint.get())
            self.result.set(f"Connected via {self.mode.get()} to {self.endpoint.get()}")
            self.connect_button.configure(text="Reconnect")
        except Exception as error:
            self.transport = None
            messagebox.showerror("Connection failed", str(error))

    def measure(self) -> None:
        if not self.transport:
            messagebox.showinfo("Not connected", "Connect to a target first.")
            return
        self.transaction_id += 1
        request = {"type": "measure_impedance", "direction": "request",
                   "transaction_id": self.transaction_id,
                   "payload": {"frequency_hz": self.frequency.get()}}
        threading.Thread(target=self._transact, args=(request,), daemon=True).start()

    def _transact(self, request: dict) -> None:
        try:
            self.events.put((True, self.transport.transact(request)))
        except Exception as error:
            self.events.put((False, error))

    def poll_events(self) -> None:
        try:
            while True:
                success, value = self.events.get_nowait()
                if not success:
                    messagebox.showerror("Measurement failed", str(value))
                    continue
                payload = value["payload"]
                impedance = complex(payload["real_ohm"], payload["imaginary_ohm"])
                self.result.set(
                    f"f = {payload['frequency_hz']:.1f} Hz\n"
                    f"Z = {impedance.real:.2f} + j{impedance.imag:.2f} Ω\n"
                    f"|Z| = {abs(impedance):.2f} Ω, phase = {cmath.phase(impedance) * 180 / 3.14159265:.2f}°"
                )
        except queue.Empty:
            pass
        self.root.after(50, self.poll_events)

    def close(self) -> None:
        if self.transport:
            self.transport.close()
        self.root.destroy()


def main() -> None:
    root = tk.Tk()
    AnalyzerGui(root)
    root.mainloop()


if __name__ == "__main__":
    main()

