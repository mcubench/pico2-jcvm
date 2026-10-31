#!/usr/bin/env python3
"""Check short-Le GET STATUS pagination through a physical PC/SC reader."""

from __future__ import annotations

import argparse
import ctypes
import os
import sys

from cryptography.hazmat.primitives import cmac
from cryptography.hazmat.primitives.ciphers import algorithms


SCOPE_SYSTEM = 2
SHARE_SHARED = 2
PROTOCOL_T1 = 2
RESET_CARD = 1
JCALGTEST_AID = bytes.fromhex("4A43416C675465737431")
STATIC_KEY = bytes.fromhex("404142434445464748494A4B4C4D4E4F")


class IoRequest(ctypes.Structure):
    _fields_ = [("protocol", ctypes.c_ulong), ("length", ctypes.c_ulong)]


class Pcsc:
    def __init__(self) -> None:
        self.lib = ctypes.CDLL("libpcsclite.so.1")
        self.context = ctypes.c_ulong()
        self.card = ctypes.c_ulong()
        self.protocol = ctypes.c_ulong()
        long = ctypes.c_long
        ulong = ctypes.c_ulong
        self.lib.SCardEstablishContext.argtypes = [
            ulong, ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ulong)]
        self.lib.SCardEstablishContext.restype = long
        self.lib.SCardListReaders.argtypes = [
            ulong, ctypes.c_char_p, ctypes.c_void_p, ctypes.POINTER(ulong)]
        self.lib.SCardListReaders.restype = long
        self.lib.SCardConnect.argtypes = [
            ulong, ctypes.c_char_p, ulong, ulong, ctypes.POINTER(ulong),
            ctypes.POINTER(ulong)]
        self.lib.SCardConnect.restype = long
        self.lib.SCardTransmit.argtypes = [
            ulong, ctypes.POINTER(IoRequest), ctypes.POINTER(ctypes.c_ubyte),
            ulong, ctypes.POINTER(IoRequest), ctypes.POINTER(ctypes.c_ubyte),
            ctypes.POINTER(ulong)]
        self.lib.SCardTransmit.restype = long
        self.lib.SCardDisconnect.argtypes = [ulong, ulong]
        self.lib.SCardDisconnect.restype = long
        self.lib.SCardReleaseContext.argtypes = [ulong]
        self.lib.SCardReleaseContext.restype = long

    def check(self, result: int, operation: str) -> None:
        if result != 0:
            raise RuntimeError(f"{operation} failed: PC/SC 0x{result & 0xffffffff:08X}")

    def connect(self, reader: str) -> None:
        self.check(self.lib.SCardEstablishContext(
            SCOPE_SYSTEM, None, None, ctypes.byref(self.context)),
            "SCardEstablishContext")
        required = ctypes.c_ulong()
        self.check(self.lib.SCardListReaders(
            self.context, None, None, ctypes.byref(required)),
            "SCardListReaders(size)")
        readers_buffer = ctypes.create_string_buffer(required.value)
        self.check(self.lib.SCardListReaders(
            self.context, None, readers_buffer, ctypes.byref(required)),
            "SCardListReaders")
        readers = [name.decode("utf-8") for name in
                   readers_buffer.raw.rstrip(b"\0").split(b"\0")]
        print("PCSC:READERS " + " | ".join(readers))
        if reader not in readers:
            raise RuntimeError(f"exact reader not present: {reader}")
        self.check(self.lib.SCardConnect(
            self.context, reader.encode(), SHARE_SHARED, PROTOCOL_T1,
            ctypes.byref(self.card), ctypes.byref(self.protocol)),
            "SCardConnect")
        if self.protocol.value != PROTOCOL_T1:
            raise RuntimeError(f"reader selected protocol {self.protocol.value}, expected T=1")
        print(f"PCSC:CONNECTED reader={reader} protocol=T=1")

    def transmit(self, command: bytes) -> bytes:
        send_buffer = (ctypes.c_ubyte * len(command)).from_buffer_copy(command)
        receive_buffer = (ctypes.c_ubyte * 4096)()
        receive_length = ctypes.c_ulong(len(receive_buffer))
        pci = IoRequest(self.protocol.value, ctypes.sizeof(IoRequest))
        self.check(self.lib.SCardTransmit(
            self.card, ctypes.byref(pci), send_buffer, len(command), None,
            receive_buffer, ctypes.byref(receive_length)), "SCardTransmit")
        response = bytes(receive_buffer[:receive_length.value])
        print(f"APDU:TX {command.hex().upper()} RX {response.hex().upper()}")
        if len(response) < 2:
            raise RuntimeError("APDU response omitted SW1/SW2")
        return response

    def close(self) -> None:
        if self.card.value:
            self.lib.SCardDisconnect(self.card, RESET_CARD)
            self.card.value = 0
        if self.context.value:
            self.lib.SCardReleaseContext(self.context)
            self.context.value = 0


def aes_cmac(key: bytes, message: bytes) -> bytes:
    mac = cmac.CMAC(algorithms.AES(key))
    mac.update(message)
    return mac.finalize()


def scp03_kdf(key: bytes, constant: int, context: bytes,
              output_bits: int) -> bytes:
    if len(context) != 16:
        raise ValueError("SCP03 context must be 16 bytes")
    data = bytearray(32)
    data[11] = constant
    data[13:15] = output_bits.to_bytes(2, "big")
    data[15] = 1
    data[16:] = context
    return aes_cmac(key, bytes(data))


class Scp03:
    def __init__(self, pcsc: Pcsc):
        self.pcsc = pcsc
        self.chaining_value = bytes(16)

    def authenticate(self) -> None:
        host_challenge = bytes.fromhex("0123456789ABCDEF")
        response = self.pcsc.transmit(
            bytes.fromhex("8050000008") + host_challenge + b"\x00")
        if len(response) != 31 or response[-2:] != b"\x90\x00":
            raise RuntimeError(f"INITIALIZE UPDATE failed: {response.hex().upper()}")
        context = host_challenge + response[13:21]
        self.session_mac = scp03_kdf(STATIC_KEY, 0x06, context, 128)
        host_cryptogram = scp03_kdf(self.session_mac, 0x01, context, 64)[:8]
        header = bytes.fromhex("8482010010")
        full_mac = aes_cmac(
            self.session_mac,
            self.chaining_value + header[:4] + b"\x10" + host_cryptogram)
        self.chaining_value = full_mac
        response = self.pcsc.transmit(header + host_cryptogram +
                                      full_mac[:8] + b"\x00")
        if response[-2:] != b"\x90\x00":
            raise RuntimeError(f"EXTERNAL AUTHENTICATE failed: {response.hex().upper()}")
        print("TEST:PASS physical SCP03 authentication")

    def get_status(self, p2: int, le: int) -> bytes:
        data = b"\x4f\x00"
        lc = len(data) + 8
        header = bytes((0x84, 0xf2, 0x40, p2, lc))
        full_mac = aes_cmac(
            self.session_mac,
            self.chaining_value + header[:4] + bytes((lc,)) + data)
        self.chaining_value = full_mac
        encoded_le = 0 if le == 256 else le
        if not 1 <= le <= 256:
            raise ValueError("Le must be in 1..256")
        return self.pcsc.transmit(
            header + data + full_mac[:8] + bytes((encoded_le,)))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--reader", required=True,
                        help="exact PC/SC reader name")
    parser.add_argument("--le", type=int, default=1,
                        help="first GET STATUS page Le (default: 1)")
    parser.add_argument("--expected-aid", action="append", default=[],
                        help="expected applet AID in the concatenated pages; repeatable")
    args = parser.parse_args()
    if os.environ.get("PICO2_HOST_USB_ACCESS") != "1":
        print("PHYSICAL_TEST:FAIL host USB access marker is required",
              file=sys.stderr)
        return 2

    pcsc = Pcsc()
    try:
        pcsc.connect(args.reader)
        # CHOUPI selects its default ISD through the case-2 empty-AID form
        # used by GlobalPlatformPro, rather than selecting an explicit AID.
        selected = pcsc.transmit(bytes.fromhex("00A4040000"))
        if selected[-2:] != b"\x90\x00":
            raise RuntimeError(f"SELECT ISD failed: {selected.hex().upper()}")
        scp = Scp03(pcsc)
        scp.authenticate()

        first = scp.get_status(0x02, args.le)
        if first[-2:] != b"\x63\x10":
            raise RuntimeError(
                f"Le={args.le} should return 6310 for this pagination probe, got "
                f"{first.hex().upper()}")
        if args.le == 1 and first[:-2]:
            raise RuntimeError(
                "Le=1 should return an empty page, got "
                f"{first.hex().upper()}")
        print(f"TEST:PASS physical GET STATUS Le={args.le} returns 6310 "
              f"with {len(first) - 2} data bytes")

        next_page = scp.get_status(0x03, 256)
        if next_page[-2:] != b"\x90\x00":
            raise RuntimeError(
                "Le=256 continuation did not complete successfully: "
                f"{next_page.hex().upper()}")
        combined_pages = first[:-2] + next_page[:-2]
        expected_aids = [bytes.fromhex(value) for value in args.expected_aid]
        if not expected_aids:
            expected_aids = [JCALGTEST_AID]
        for aid in expected_aids:
            encoded_aid = bytes((0x4f, len(aid))) + aid
            if combined_pages.count(encoded_aid) != 1:
                raise RuntimeError(
                    "GET STATUS pages omitted or duplicated applet AID "
                    f"{aid.hex().upper()}: count="
                    f"{combined_pages.count(encoded_aid)}")
        print("TEST:PASS physical GET STATUS continuation contains all "
              f"{len(expected_aids)} expected applet AIDs exactly once")
        print("TEST:SUMMARY pass=2 fail=0")
        return 0
    except Exception as exc:  # Report transport and protocol faults uniformly.
        print(f"TEST:FAIL physical_get_status_le {exc}", file=sys.stderr)
        return 1
    finally:
        pcsc.close()


if __name__ == "__main__":
    raise SystemExit(main())
