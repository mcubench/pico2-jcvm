"""Fail-fast check shared by Python tools that access host USB devices."""

from pathlib import Path
import sys


MESSAGE = (
    "USB/PCSC access is unavailable in this restricted context; "
    "rerun via the host-access execution path"
)


def require_host_usb_access():
    """Exit before USB/PCSC calls unless the execution context is verified."""
    try:
        status = Path("/proc/self/status").read_text(encoding="ascii")
    except OSError:
        status = ""

    for line in status.splitlines():
        if line.startswith("NoNewPrivs:"):
            fields = line.split()
            if len(fields) == 2 and fields[1] == "0":
                return
            else:
                print(f"ERROR: {MESSAGE}", file=sys.stderr, flush=True)
                raise SystemExit(1)

    print(
        f"ERROR: cannot verify USB/PCSC execution context; {MESSAGE}",
        file=sys.stderr,
        flush=True,
    )
    raise SystemExit(1)
