"""Fail-fast check shared by Python tools that access host USB devices."""

from pathlib import Path
import sys


MESSAGE = (
    "USB/PCSC access is unavailable in this restricted context; "
    "rerun via the host-access execution path"
)


def require_host_usb_access():
    """Exit before libusb/PCSC calls when running under the restricted sandbox."""
    try:
        status = Path("/proc/self/status").read_text(encoding="ascii")
    except OSError:
        return

    for line in status.splitlines():
        if line.startswith("NoNewPrivs:"):
            if line.split()[1] == "1":
                print(f"ERROR: {MESSAGE}", file=sys.stderr, flush=True)
                raise SystemExit(1)
            return
