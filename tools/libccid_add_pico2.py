#!/usr/bin/env python3
"""Add the Pico 2 CHOUPI CCID reader (VID 0x2E8A, PID 0x0009) to a libccid Info.plist.

Usage: libccid_add_pico2.py PLIST_IN PLIST_OUT
Appends one entry to each of the parallel arrays ifdVendorID, ifdProductID and
ifdFriendlyName, only if the VID/PID pair is absent. Validates the result with
plistlib before writing PLIST_OUT. Exit 0 = written (changed or already present).
"""
import plistlib
import re
import sys

VID, PID, NAME = "0x2E8A", "0x0009", "Pico 2 CHOUPI CCID"


def pair_present(data):
    vids = [v.lower() for v in data["ifdVendorID"]]
    pids = [p.lower() for p in data["ifdProductID"]]
    return any(v == VID.lower() and p == PID.lower() for v, p in zip(vids, pids))


def main():
    src, dst = sys.argv[1], sys.argv[2]
    text = open(src, encoding="utf-8").read()
    before = plistlib.loads(text.encode("utf-8"))
    n = len(before["ifdVendorID"])
    if not (n == len(before["ifdProductID"]) == len(before["ifdFriendlyName"])):
        sys.exit("parallel arrays differ in length; refusing to edit")
    if pair_present(before):
        open(dst, "w", encoding="utf-8").write(text)
        print("already-present entries=%d" % n)
        return
    for key, value in (("ifdVendorID", VID), ("ifdProductID", PID),
                       ("ifdFriendlyName", NAME)):
        m = re.search(r"<key>%s</key>\s*<array>.*?(\n[ \t]*)</array>" % key, text, re.S)
        if not m:
            sys.exit("array %s not found" % key)
        indent = m.group(1)
        # Insert before the closing tag, one tab deeper than the closing tag.
        entry = "%s\t<string>%s</string>" % (indent, value)
        text = text[: m.start(1)] + entry + text[m.start(1):]
    after = plistlib.loads(text.encode("utf-8"))
    ok = (len(after["ifdVendorID"]) == len(after["ifdProductID"])
          == len(after["ifdFriendlyName"]) == n + 1 and pair_present(after)
          and after["ifdFriendlyName"][-1] == NAME
          and after["ifdVendorID"][:n] == before["ifdVendorID"]
          and after["ifdFriendlyName"][:n] == before["ifdFriendlyName"])
    if not ok:
        sys.exit("validation failed; nothing written")
    open(dst, "w", encoding="utf-8").write(text)
    print("added entries=%d->%d" % (n, n + 1))


if __name__ == "__main__":
    main()
