from __future__ import annotations

import json
import socket
from abc import ABC, abstractmethod
from typing import Any


class Transport(ABC):
    @abstractmethod
    def transact(self, message: dict[str, Any]) -> dict[str, Any]: ...

    @abstractmethod
    def close(self) -> None: ...


class TcpTransport(Transport):
    def __init__(self, host: str, port: int, timeout: float = 2.0):
        self._socket = socket.create_connection((host, port), timeout=timeout)
        self._file = self._socket.makefile("rwb")

    def transact(self, message: dict[str, Any]) -> dict[str, Any]:
        self._file.write(json.dumps(message, separators=(",", ":")).encode() + b"\n")
        self._file.flush()
        line = self._file.readline()
        if not line:
            raise ConnectionError("target closed the connection")
        return json.loads(line)

    def close(self) -> None:
        self._file.close()
        self._socket.close()


class SerialTransport(Transport):
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 2.0):
        try:
            import serial
        except ImportError as exc:
            raise RuntimeError("install serial support with: pip install -e '.[serial]'") from exc
        self._serial = serial.Serial(port, baudrate=baudrate, timeout=timeout)

    def transact(self, message: dict[str, Any]) -> dict[str, Any]:
        self._serial.write(json.dumps(message, separators=(",", ":")).encode() + b"\n")
        line = self._serial.readline()
        if not line:
            raise TimeoutError("target did not respond")
        return json.loads(line)

    def close(self) -> None:
        self._serial.close()

