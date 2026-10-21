from .base import TelemetrySource
from .demo import DemoSource
from .spa import SpaSource

SOURCES = {"demo": DemoSource, "spa": SpaSource}

__all__ = ["SOURCES", "DemoSource", "SpaSource", "TelemetrySource"]
