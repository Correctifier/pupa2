from __future__ import annotations
import json, socket, threading
from abc import ABC, abstractmethod
from typing import Any


class Transport(ABC):
    """NDJSON message transport with no analyzer protocol semantics."""

    @abstractmethod
    def send(self, message: dict[str, Any]) -> None: ...

    @abstractmethod
    def receive(self) -> dict[str, Any]: ...

    @abstractmethod
    def close(self) -> None: ...


class TcpTransport(Transport):
    def __init__(
        self,
        host: str,
        port: int,
        timeout: float = 2.0,
    ):
        self._socket = socket.create_connection((host, port), timeout=timeout)

        self._socket.settimeout(None)

        self._reader = self._socket.makefile("rb")
        self._send_lock = threading.Lock()

    def send(self, message: dict[str, Any]) -> None:
        data = json.dumps(message, separators=(",", ":")).encode() + b"\n"

        with self._send_lock:
            self._socket.sendall(data)

    def receive(self) -> dict[str, Any]:
        line = self._reader.readline()

        if not line:
            raise ConnectionError("target closed the connection")

        try:
            value = json.loads(line)
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ValueError(f"invalid NDJSON message: {error}") from error

        if not isinstance(value, dict):
            raise ValueError("protocol message must be a JSON object")

        return value

    def close(self) -> None:
        try:
            self._socket.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass

        self._reader.close()
        self._socket.close()


class SerialTransport(Transport):
    def __init__(self, port: str, baudrate: int = 115200):
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError(
                "install serial support with: sudo apt install python3-serial"
            ) from exc

        self._serial = serial.Serial(
            port,
            baudrate=baudrate,
            timeout=None,
        )
        self._send_lock = threading.Lock()

    def send(self, message: dict[str, Any]) -> None:
        data = json.dumps(message, separators=(",", ":")).encode() + b"\n"

        with self._send_lock:
            self._serial.write(data)
            self._serial.flush()

    def receive(self) -> dict[str, Any]:
        line = self._serial.readline()

        if not line:
            raise ConnectionError("serial target disconnected")

        try:
            value = json.loads(line)
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ValueError(f"invalid NDJSON message: {error}") from error

        if not isinstance(value, dict):
            raise ValueError("protocol message must be a JSON object")

        return value

    def close(self) -> None:
        self._serial.close()
