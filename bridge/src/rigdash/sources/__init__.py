from .base import TelemetrySource
from .demo import DemoSource

SOURCES = {"demo": DemoSource}

__all__ = ["SOURCES", "DemoSource", "TelemetrySource"]
