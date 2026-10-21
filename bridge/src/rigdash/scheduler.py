"""Decides which frames go out when, so slow data doesn't eat bandwidth."""

from typing import Optional

from .model import Config, Snapshot
from .protocol import (
    encode_car, encode_config, encode_engine, encode_fuel, encode_hello, encode_lap_history,
    encode_race, encode_telemetry, encode_timing, encode_track_info, encode_tyres, encode_weather,
)

# Minimum interval per message in ms. Timing and history also go out on change.
INTERVALS_MS = {
    "tyres": 200, "car": 200, "race": 200, "fuel": 500, "weather": 1000,
    "timing": 1000, "history": 2000,
}
ENCODERS = {
    "engine": encode_engine, "tyres": encode_tyres, "car": encode_car, "fuel": encode_fuel,
    "race": encode_race, "timing": encode_timing, "weather": encode_weather,
    "history": encode_lap_history,
}
HELLO_INTERVAL_MS = 2000


class FrameScheduler:
    def __init__(self, config: Optional[Config] = None, track: str = "") -> None:
        self.config = config or Config()
        self.track = track
        self._last: dict[str, int] = {}
        self._last_payload: dict[str, bytes] = {}

    def _due(self, key: str, now: int, interval: int) -> bool:
        last = self._last.get(key)
        return last is None or now - last >= interval

    def frames(self, snap: Optional[Snapshot], now_ms: int) -> list[bytes]:
        out: list[bytes] = []
        if self._due("hello", now_ms, HELLO_INTERVAL_MS):
            self._last["hello"] = now_ms
            out += [encode_hello(), encode_config(self.config)]
            if self.track:
                out.append(encode_track_info(self.track))
        if snap is None:
            return out
        out.append(encode_telemetry(snap.telemetry))
        if snap.engine is not None:
            out.append(encode_engine(snap.engine))
        for key, interval in INTERVALS_MS.items():
            part = getattr(snap, key)
            if part is None:
                continue
            frame = ENCODERS[key](part)
            changed = key in ("timing", "history") and frame != self._last_payload.get(key)
            if changed or self._due(key, now_ms, interval):
                out.append(frame)
                self._last[key] = now_ms
                self._last_payload[key] = frame
        return out
