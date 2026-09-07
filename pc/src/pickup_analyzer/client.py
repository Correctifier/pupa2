from __future__ import annotations
import queue, threading
from collections.abc import Callable
from typing import Any
from .messages import Measurement
from .transport import Transport


class ProtocolError(RuntimeError):
    def __init__(self, message: str, code: str = "protocol_error"):
        super().__init__(message)

        self.code = code


class AnalyzerClient:
    """Protocol semantics, transaction matching, and asynchronous event routing."""

    def __init__(self, transport: Transport, monitor: Callable[[str, dict], None] | None = None):
        self.transport, self.monitor = transport, monitor
        self._next_id = 1
        self._id_lock = threading.Lock()
        self._pending: dict[int, queue.Queue] = {}
        self._pending_lock = threading.Lock()
        self._events: queue.Queue[dict[str, Any]] = queue.Queue()
        self._closed = threading.Event()
        self._reader = threading.Thread(target=self._receive_loop, daemon=True)

        self._reader.start()

    def request(
        self,
        object_name: str,
        action: str,
        params: dict[str, Any] | None = None,
        timeout: float = 5.0,
    ) -> dict[str, Any]:
        with self._id_lock:
            transaction_id, self._next_id = self._next_id, self._next_id + 1

        destination: queue.Queue = queue.Queue(maxsize=1)

        with self._pending_lock:
            self._pending[transaction_id] = destination

        message: dict[str, Any] = {
            "type": "request",
            "object": object_name,
            "action": action,
            "id": transaction_id,
        }

        if params is not None:
            message["params"] = params

        try:
            if self.monitor:
                self.monitor("TX", message)

            self.transport.send(message)

            try:
                response = destination.get(timeout=timeout)
            except queue.Empty as error:
                raise TimeoutError(f"request {transaction_id} timed out") from error
        finally:
            with self._pending_lock:
                self._pending.pop(transaction_id, None)

        if isinstance(response, Exception):
            raise response

        if response.get("type") == "error" or response.get("status") == "error":
            detail = response.get("error", {})

            raise ProtocolError(
                detail.get("message", "target rejected request"),
                detail.get("code", "target_error"),
            )

        return response

    def device_info(self):
        return self.request("device", "info").get("data", {})

    def set_generator(self, frequency, amplitude):
        self.request(
            "generator",
            "set",
            {"frequency": frequency, "amplitude": amplitude},
        )

    def start_sweep(
        self,
        start_hz,
        stop_hz,
        points,
    ):
        self.request(
            "sweep",
            "start",
            {
                "f_start": start_hz,
                "f_stop": stop_hz,
                "points": points,
            },
        )

    def stop_sweep(self):
        self.request("sweep", "stop")

    def discard_events(self):
        """Drain stale events after an acknowledged stop; use with one sweep consumer."""

        while True:
            try:
                self._events.get_nowait()
            except queue.Empty:
                return

    def set_range_auto(self):
        self.request(
            "range",
            "set",
            {"mode": "auto"},
        )

    def set_range_manual(self, index):
        self.request(
            "range",
            "set",
            {"mode": "manual", "range": index},
        )

    def run_calibration(self):
        self.request(
            "calibration",
            "run",
            timeout=30.0,
        )

    def profiler_threads(self):
        return self.request("profiler", "threads").get("data", [])

    def profiler_data(self):
        return self.request("profiler", "data").get("data", [])

    def reset_profiler(self):
        self.request("profiler", "reset")

    def next_event(self, timeout=None):
        event = self._events.get(timeout=timeout)

        if event.get("type") == "error":
            detail = event.get("error", {})

            raise ProtocolError(
                detail.get("message", "target error"),
                detail.get("code", "target_error"),
            )

        return event

    def next_measurement(self, timeout=None):
        while True:
            event = self.next_event(timeout)

            if event.get("object") == "measurement":
                return Measurement.from_event(event)

    def _receive_loop(self):
        try:
            while not self._closed.is_set():
                message = self.transport.receive()

                if self.monitor:
                    self.monitor("RX", message)

                if message.get("type") in ("response", "error") and isinstance(
                    message.get("id"),
                    int,
                ):
                    with self._pending_lock:
                        destination = self._pending.get(message["id"])

                    if destination:
                        destination.put(message)
                    elif message.get("type") == "error":
                        self._events.put(message)
                elif message.get("type") in ("event", "error"):
                    self._events.put(message)
        except Exception as error:
            if not self._closed.is_set():
                with self._pending_lock:
                    destinations = list(self._pending.values())

                for destination in destinations:
                    destination.put(error)

                self._events.put({
                    "type": "error",
                    "error": {"code": "transport_disconnected", "message": str(error)},
                })

    def close(self):
        self._closed.set()
        self.transport.close()
