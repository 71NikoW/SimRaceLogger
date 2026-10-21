from abc import ABC, abstractmethod
from typing import Optional

from ..model import Snapshot


class TelemetrySource(ABC):
    """One implementation per game. Must not block for long in poll().

    Fill in what the game exposes and leave the rest as None / defaults; the
    bridge computes fuel per lap, sectors and lap history if missing.
    """

    @abstractmethod
    def poll(self, now_ms: int) -> Optional[Snapshot]:
        """Return the latest data, or None if the game isn't running."""

    def track_name(self) -> str:
        return ""

    def close(self) -> None:
        pass
