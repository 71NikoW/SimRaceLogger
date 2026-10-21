from abc import ABC, abstractmethod
from typing import Optional

from ..model import Telemetry


class TelemetrySource(ABC):
    """One implementation per game. Must not block for long in poll()."""

    @abstractmethod
    def poll(self, now_ms: int) -> Optional[Telemetry]:
        """Return the latest telemetry, or None if the game isn't running."""

    def track_name(self) -> str:
        return ""

    def close(self) -> None:
        pass
