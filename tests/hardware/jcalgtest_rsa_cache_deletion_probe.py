#!/usr/bin/env python3
"""Reproduce the JCAlgTest RSA key-cache growth and time object deletion.

JCAlgTest's AlgPerformanceTest caches two key pairs per RSA type and length
(getKeyPairObject) and keeps them for the rest of the run. Every B0E2
(JCSystem.requestObjectDeletion) then has to scan a larger live set. This
probe sends the exact B031 prepare APDUs that the 1.8.5 client sends for
TYPE_RSA_PRIVATE and TYPE_RSA_CRT_PRIVATE NOPAD Cipher_doFinal (taken from
the Windows de729d28 log), each followed by B0E2 and one B043 measure, and
prints the elapsed time of every APDU.

It talks raw CCID over libusb and selects the token by its USB serial (chip
ID), so it never touches pcscd or readers that pcscd owns. CCID time
extensions (bmCommandStatus=2) are followed; a 30 s silence or a vanished
device is reported as DEVICE_LOST.

Run: PICO2_HOST_USB_ACCESS=1 tests/hardware/jcalgtest_rsa_cache_deletion_probe.py \
         --serial F42DD9D07D6D2B07 [--sizes 512,736,...] [--types private,crt]
"""

from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from host_usb_access import require_host_usb_access  # noqa: E402

VID = 0x2E8A
PID = 0x0009
CCID_INTERFACE = 2
ENDPOINT_OUT = 0x03
ENDPOINT_IN = 0x83
LIBUSB_ERROR_TIMEOUT = -7
LIBUSB_ERROR_NO_DEVICE = -4

SELECT = "00A404000A4A43416C67546573743100"
DELETE_OBJECTS = "B0E2000000"
# Key length field (bytes 13..14 of the B031 data) per RSA length.
LENGTHS = {
    512: "0200", 736: "02E0", 768: "0300", 896: "0380", 1024: "0400",
    1280: "0500", 1536: "0600", 1984: "07C0", 2048: "0800", 3072: "0C00",
}
# keyClass byte: 05 = TYPE_RSA_PRIVATE (KeyPair.ALG_RSA), 06 = CRT private.
TYPES = {"private": "05", "crt": "06"}


def prepare_apdu(key_class: str, length: int) -> str:
    bits = LENGTHS[length]
    return ("B0310000160011000CFFFF00" + key_class + bits + "000700"
            + f"{length // 8:02X}" + "FFFF00010032000100")


def measure_apdu(key_class: str, length: int) -> str:
    return ("B0430000160011000CFFFF00" + key_class + LENGTHS[length] + "000700"
            + f"{length // 8:02X}" + "FFFF00010000000100")


class DeviceLost(RuntimeError):
    pass


class DeviceDescriptor(ctypes.Structure):
    _fields_ = [
        ("length", ctypes.c_uint8), ("descriptor_type", ctypes.c_uint8),
        ("usb_version", ctypes.c_uint16), ("device_class", ctypes.c_uint8),
        ("device_subclass", ctypes.c_uint8),
        ("device_protocol", ctypes.c_uint8),
        ("max_packet_size", ctypes.c_uint8), ("vendor_id", ctypes.c_uint16),
        ("product_id", ctypes.c_uint16), ("device_version", ctypes.c_uint16),
        ("manufacturer_index", ctypes.c_uint8),
        ("product_index", ctypes.c_uint8), ("serial_index", ctypes.c_uint8),
        ("configuration_count", ctypes.c_uint8),
    ]


class RawCcid:
    def __init__(self, serial: str):
        name = ctypes.util.find_library("usb-1.0")
        if not name:
            raise RuntimeError("libusb-1.0 is not installed")
        lib = ctypes.CDLL(name)
        self.lib = lib
        vp = ctypes.c_void_p
        lib.libusb_init.argtypes = [ctypes.POINTER(vp)]
        lib.libusb_get_device_list.argtypes = [vp, ctypes.POINTER(
            ctypes.POINTER(vp))]
        lib.libusb_get_device_list.restype = ctypes.c_ssize_t
        lib.libusb_free_device_list.argtypes = [ctypes.POINTER(vp),
                                                ctypes.c_int]
        lib.libusb_get_device_descriptor.argtypes = [
            vp, ctypes.POINTER(DeviceDescriptor)]
        lib.libusb_open.argtypes = [vp, ctypes.POINTER(vp)]
        lib.libusb_close.argtypes = [vp]
        lib.libusb_exit.argtypes = [vp]
        lib.libusb_get_string_descriptor_ascii.argtypes = [
            vp, ctypes.c_uint8, ctypes.POINTER(ctypes.c_uint8), ctypes.c_int]
        lib.libusb_claim_interface.argtypes = [vp, ctypes.c_int]
        lib.libusb_release_interface.argtypes = [vp, ctypes.c_int]
        lib.libusb_bulk_transfer.argtypes = [
            vp, ctypes.c_ubyte, ctypes.POINTER(ctypes.c_ubyte), ctypes.c_int,
            ctypes.POINTER(ctypes.c_int), ctypes.c_uint]
        self.context = vp()
        if lib.libusb_init(ctypes.byref(self.context)) != 0:
            raise RuntimeError("libusb_init failed")
        self.handle = None
        devices = ctypes.POINTER(vp)()
        count = lib.libusb_get_device_list(self.context, ctypes.byref(devices))
        try:
            for index in range(max(0, count)):
                device = devices[index]
                descriptor = DeviceDescriptor()
                if lib.libusb_get_device_descriptor(
                        device, ctypes.byref(descriptor)) != 0:
                    continue
                if (descriptor.vendor_id, descriptor.product_id) != (VID, PID):
                    continue
                handle = vp()
                if lib.libusb_open(device, ctypes.byref(handle)) != 0:
                    continue
                raw = (ctypes.c_uint8 * 64)()
                n = lib.libusb_get_string_descriptor_ascii(
                    handle, descriptor.serial_index, raw, len(raw))
                found = bytes(raw[:max(0, n)]).decode("ascii", "replace")
                if found.upper() == serial.upper():
                    self.handle = handle
                    break
                lib.libusb_close(handle)
        finally:
            lib.libusb_free_device_list(devices, 1)
        if self.handle is None:
            lib.libusb_exit(self.context)
            raise RuntimeError(f"USB device with serial {serial} not found")
        status = lib.libusb_claim_interface(self.handle, CCID_INTERFACE)
        if status != 0:
            self.close()
            raise RuntimeError(
                f"claim CCID interface failed ({status}); is pcscd using it?")
        self.sequence = 0

    def bulk(self, endpoint, data, length, timeout_ms):
        buffer = (ctypes.c_ubyte * max(1, length))()
        if data:
            for index, value in enumerate(data):
                buffer[index] = value
        transferred = ctypes.c_int()
        status = self.lib.libusb_bulk_transfer(
            self.handle, endpoint, buffer, length, ctypes.byref(transferred),
            timeout_ms)
        if status in (LIBUSB_ERROR_TIMEOUT, LIBUSB_ERROR_NO_DEVICE) or (
                status != 0 and status < 0):
            raise DeviceLost(f"bulk 0x{endpoint:02X} status={status}")
        return bytes(buffer[:transferred.value])

    def command(self, kind, payload=b""):
        """Send one CCID message; return (response, time_extensions)."""
        sequence = self.sequence
        header = (bytes((kind,)) + struct.pack("<I", len(payload))
                  + bytes((0, sequence, 0, 0, 0)))
        self.bulk(ENDPOINT_OUT, header + payload, len(header) + len(payload),
                  5000)
        extensions = 0
        while True:
            response = self.bulk(ENDPOINT_IN, None, 512, 30000)
            if len(response) < 10 or response[6] != sequence:
                raise RuntimeError("bad CCID response " + response.hex())
            if (response[7] & 0xC0) == 0x80:  # time extension
                extensions += 1
                continue
            self.sequence = (sequence + 1) & 0xFF
            return response, extensions

    def transmit(self, apdu_hex):
        response, extensions = self.command(0x6F, bytes.fromhex(apdu_hex))
        if response[0] != 0x80 or response[7] != 0:
            raise RuntimeError("CCID transfer failed " + response.hex())
        return response[10:], extensions

    def close(self):
        if self.handle:
            self.lib.libusb_release_interface(self.handle, CCID_INTERFACE)
            self.lib.libusb_close(self.handle)
            self.handle = None
        if self.context:
            self.lib.libusb_exit(self.context)
            self.context = None


def run_apdu(card, label, apdu_hex, results):
    start = time.monotonic()
    try:
        data, extensions = card.transmit(apdu_hex)
    except DeviceLost as error:
        elapsed = int((time.monotonic() - start) * 1000)
        print(f"APDU:DEVICE_LOST label={label} elapsed_ms={elapsed} "
              f"error={error}", flush=True)
        raise
    elapsed = int((time.monotonic() - start) * 1000)
    sw = data[-2:].hex().upper()
    body = data[:-2].hex().upper()
    print(f"APDU:RESULT label={label} sw={sw} data={body} "
          f"elapsed_ms={elapsed} time_extensions={extensions}", flush=True)
    results.append((label, sw, elapsed))
    return sw, body


def main():
    require_host_usb_access()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True)
    parser.add_argument("--sizes", default=",".join(str(s) for s in LENGTHS))
    parser.add_argument("--types", default="private,crt")
    parser.add_argument("--deletions", type=int, default=2,
                        help="B0E2 APDUs after each prepare")
    parser.add_argument("--skip-prepare", action="store_true",
                        help="only time B0E2 on the existing cache")
    args = parser.parse_args()
    sizes = [int(s) for s in args.sizes.split(",") if s]
    types = [t for t in args.types.split(",") if t]

    card = RawCcid(args.serial)
    results = []
    try:
        response, _ = card.command(0x62)
        print("CCID:POWER_ON atr=" + response[10:].hex().upper(), flush=True)
        run_apdu(card, "SELECT", SELECT, results)
        if args.skip_prepare:
            for index in range(args.deletions):
                run_apdu(card, f"B0E2_{index}", DELETE_OBJECTS, results)
            return 0
        for key_type in types:
            for size in sizes:
                tag = f"{key_type}_{size}"
                sw, body = run_apdu(card, f"PREPARE_{tag}",
                                    prepare_apdu(TYPES[key_type], size),
                                    results)
                for index in range(args.deletions):
                    run_apdu(card, f"B0E2_{tag}_{index}", DELETE_OBJECTS,
                             results)
                if sw == "9000" and body == "AA":
                    run_apdu(card, f"MEASURE_{tag}",
                             measure_apdu(TYPES[key_type], size), results)
        print("PROBE:PASS apdus=%d" % len(results), flush=True)
        return 0
    except DeviceLost:
        print("PROBE:FAIL reason=device_lost apdus=%d" % len(results),
              flush=True)
        return 1
    finally:
        card.close()


if __name__ == "__main__":
    sys.exit(main())
