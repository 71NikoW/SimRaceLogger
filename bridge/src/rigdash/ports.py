"""Find the display's serial port automatically."""

from typing import Iterable, Optional

ESPRESSIF_VID = 0x303A  # native USB of ESP32-S2/S3
USB_UART_VIDS = {0x10C4, 0x1A86, 0x0403}  # CP210x, CH340, FTDI on dev boards


def find_display_ports(ports: Optional[Iterable] = None) -> list[str]:
    """All devices of this project (display, flag panel, button box).

    Native-USB ESP32-S3 boards are preferred; USB-UART bridges only count when
    no native one is present.
    """
    if ports is None:
        from serial.tools import list_ports  # type: ignore
        ports = list_ports.comports()
    ports = list(ports)
    for want in ({ESPRESSIF_VID}, USB_UART_VIDS):
        hits = sorted(p.device for p in ports if getattr(p, "vid", None) in want)
        if hits:
            return hits
    return []


def find_display_port(ports: Optional[Iterable] = None) -> Optional[str]:
    """Return the device name of the most likely display, or None.

    `ports` are pyserial ListPortInfo-like objects (.device, .vid); by default
    the system's ports are listed.
    """
    if ports is None:
        from serial.tools import list_ports  # type: ignore
        ports = list_ports.comports()
    ports = list(ports)
    for want in ({ESPRESSIF_VID}, USB_UART_VIDS):
        hits = sorted(p.device for p in ports if getattr(p, "vid", None) in want)
        if hits:
            return hits[0]
    return None
