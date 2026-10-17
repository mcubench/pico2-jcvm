"""Fail-fast check shared by Python tools that access host USB devices."""

from pathlib import Path
import os
import sys


def require_host_usb_access():
    """Exit before USB/PCSC calls unless the execution context is verified."""
    if os.environ.get("PICO2_HOST_USB_ACCESS") != "1":
        print(
            "ERROR: USB/PCSC commands require the explicit host-access marker; "
            "rerun this command with sandbox_permissions=require_escalated "
            "and PICO2_HOST_USB_ACCESS=1",
            file=sys.stderr,
            flush=True,
        )
        raise SystemExit(1)

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
                print(
                    "ERROR: USB/PCSC access is unavailable in this restricted "
                    "context; rerun with sandbox_permissions=require_escalated",
                    file=sys.stderr,
                    flush=True,
                )
                raise SystemExit(1)

    print(
        "ERROR: cannot verify USB/PCSC execution context; rerun with "
        "sandbox_permissions=require_escalated",
        file=sys.stderr,
        flush=True,
    )
    raise SystemExit(1)
