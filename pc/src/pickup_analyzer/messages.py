from __future__ import annotations
from dataclasses import dataclass
from typing import Any

@dataclass(frozen=True)
class ComplexValue:
    re: float
    im: float
    @classmethod
    def from_dict(cls, value: dict[str, Any]) -> "ComplexValue":
        return cls(float(value["re"]), float(value["im"]))

@dataclass(frozen=True)
class Measurement:
    frequency_hz: float
    range_index: int
    sense_resistor_ohm: float
    v: ComplexValue
    vsense: ComplexValue
    v_min: int
    v_max: int
    vsense_min: int
    vsense_max: int
    z: ComplexValue
    @classmethod
    def from_event(cls, message: dict[str, Any]) -> "Measurement":
        if message.get("type") != "event" or message.get("object") != "measurement":
            raise ValueError("message is not a measurement event")
        data = message["data"]
        return cls(float(data["f"]), int(data["range"]), float(data["rsense"]),
                   ComplexValue.from_dict(data["v"]), ComplexValue.from_dict(data["vsense"]),
                   int(data["v_min"]), int(data["v_max"]), int(data["vsense_min"]),
                   int(data["vsense_max"]), ComplexValue.from_dict(data["z"]))

