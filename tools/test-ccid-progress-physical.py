#!/usr/bin/env python3
"""Exercise RP2350 control transfers during a physical long RSA APDU."""

from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import os
from pathlib import Path
import signal
import subprocess
import sys
import threading
import time

from host_usb_access import require_host_usb_access


ROOT = Path(__file__).resolve().parents[1]
VID = 0x2E8A
PID = 0x0009
DEVICE_DESCRIPTOR_REQUEST = (0x80, 0x06, 0x0100, 0x0000, 18)
REQUEST_INTERVAL_SECONDS = 0.02


class DeviceDescriptor(ctypes.Structure):
    _fields_ = [
        ("length", ctypes.c_uint8),
        ("descriptor_type", ctypes.c_uint8),
        ("usb_version", ctypes.c_uint16),
        ("device_class", ctypes.c_uint8),
        ("device_subclass", ctypes.c_uint8),
        ("device_protocol", ctypes.c_uint8),
        ("max_packet_size", ctypes.c_uint8),
        ("vendor_id", ctypes.c_uint16),
        ("product_id", ctypes.c_uint16),
        ("device_version", ctypes.c_uint16),
        ("manufacturer_index", ctypes.c_uint8),
        ("product_index", ctypes.c_uint8),
        ("serial_index", ctypes.c_uint8),
        ("configuration_count", ctypes.c_uint8),
    ]


class LibusbDevice(ctypes.Structure):
    pass


DevicePointer = ctypes.POINTER(LibusbDevice)
DeviceList = ctypes.POINTER(DevicePointer)


def open_target(serial: str):
    library = ctypes.util.find_library("usb-1.0")
    if not library:
        raise RuntimeError("libusb-1.0 is not installed")
    libusb = ctypes.CDLL(library)
    libusb.libusb_init.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
    libusb.libusb_init.restype = ctypes.c_int
    libusb.libusb_get_device_list.argtypes = [ctypes.c_void_p,
                                               ctypes.POINTER(DeviceList)]
    libusb.libusb_get_device_list.restype = ctypes.c_ssize_t
    libusb.libusb_free_device_list.argtypes = [DeviceList, ctypes.c_int]
    libusb.libusb_get_device_descriptor.argtypes = [
        DevicePointer, ctypes.POINTER(DeviceDescriptor)]
    libusb.libusb_get_device_descriptor.restype = ctypes.c_int
    libusb.libusb_open.argtypes = [DevicePointer,
                                   ctypes.POINTER(ctypes.c_void_p)]
    libusb.libusb_open.restype = ctypes.c_int
    libusb.libusb_close.argtypes = [ctypes.c_void_p]
    libusb.libusb_get_string_descriptor_ascii.argtypes = [
        ctypes.c_void_p, ctypes.c_uint8,
        ctypes.POINTER(ctypes.c_uint8), ctypes.c_int]
    libusb.libusb_get_string_descriptor_ascii.restype = ctypes.c_int
    libusb.libusb_control_transfer.argtypes = [
        ctypes.c_void_p, ctypes.c_uint8, ctypes.c_uint8,
        ctypes.c_uint16, ctypes.c_uint16, ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_uint16, ctypes.c_uint]
    libusb.libusb_control_transfer.restype = ctypes.c_int
    libusb.libusb_exit.argtypes = [ctypes.c_void_p]
    libusb.libusb_get_bus_number.argtypes = [DevicePointer]
    libusb.libusb_get_bus_number.restype = ctypes.c_uint8
    libusb.libusb_get_device_address.argtypes = [DevicePointer]
    libusb.libusb_get_device_address.restype = ctypes.c_uint8

    context = ctypes.c_void_p()
    status = libusb.libusb_init(ctypes.byref(context))
    if status != 0:
        raise RuntimeError(f"libusb_init returned {status}")
    devices = DeviceList()
    count = libusb.libusb_get_device_list(context, ctypes.byref(devices))
    if count < 0:
        libusb.libusb_exit(context)
        raise RuntimeError(f"libusb_get_device_list returned {count}")

    target_handle = None
    target_location = ""
    try:
        for index in range(count):
            device = devices[index]
            descriptor = DeviceDescriptor()
            status = libusb.libusb_get_device_descriptor(
                device, ctypes.byref(descriptor))
            if status != 0 or (descriptor.vendor_id, descriptor.product_id) != (
                    VID, PID):
                continue
            handle = ctypes.c_void_p()
            status = libusb.libusb_open(device, ctypes.byref(handle))
            if status != 0:
                continue
            raw_serial = (ctypes.c_uint8 * 256)()
            serial_length = libusb.libusb_get_string_descriptor_ascii(
                handle, descriptor.serial_index, raw_serial, len(raw_serial))
            found_serial = (bytes(raw_serial[:serial_length]).decode(
                "ascii", "replace") if serial_length > 0 else "")
            if found_serial.upper() == serial.upper():
                target_handle = handle
                target_location = (
                    f"{libusb.libusb_get_bus_number(device)}:"
                    f"{libusb.libusb_get_device_address(device)}")
                break
            libusb.libusb_close(handle)
    finally:
        libusb.libusb_free_device_list(devices, 1)

    if target_handle is None:
        libusb.libusb_exit(context)
        raise RuntimeError(f"USB reader {VID:04x}:{PID:04x} serial {serial} not found")
    return libusb, context, target_handle, target_location


class ControlTraffic:
    def __init__(self, libusb, handle):
        self.libusb = libusb
        self.handle = handle
        self.lock = threading.Lock()
        self.stop_event = threading.Event()
        self.thread = None
        self.requests = 0
        self.successes = 0
        self.failures = 0
        self.total_latency_us = 0
        self.max_latency_us = 0
        self.apdu_start = None
        self.apdu_end = None
        self.success_start_times = []

    def request(self):
        control = (ctypes.c_uint8 * 18)()
        start = time.monotonic_ns()
        request_type, request, value, index, length = DEVICE_DESCRIPTOR_REQUEST
        result = self.libusb.libusb_control_transfer(
            self.handle, request_type, request, value, index, control, length,
            2000)
        elapsed_us = (time.monotonic_ns() - start) // 1000
        okay = (result == length and control[0] == 18 and control[1] == 1)
        with self.lock:
            self.requests += 1
            self.total_latency_us += elapsed_us
            self.max_latency_us = max(self.max_latency_us, elapsed_us)
            if okay:
                self.successes += 1
                self.success_start_times.append(start)
            else:
                self.failures += 1
        return okay, elapsed_us, result

    def start(self):
        okay, elapsed_us, result = self.request()
        print(f"USB_CONTROL:PREFLIGHT status={'PASS' if okay else 'FAIL'} "
              f"response_bytes={result} latency_us={elapsed_us}", flush=True)
        if not okay:
            raise RuntimeError("USB device-descriptor control request failed")
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def _run(self):
        while not self.stop_event.is_set():
            self.request()
            self.stop_event.wait(REQUEST_INTERVAL_SECONDS)

    def mark_apdu(self, start: bool):
        with self.lock:
            if start:
                self.apdu_start = time.monotonic_ns()
                self.apdu_end = None
            else:
                self.apdu_end = time.monotonic_ns()

    def stop_and_drain(self):
        self.stop_event.set()
        if self.thread is not None:
            self.thread.join()
        drain_success = 0
        for _ in range(5):
            okay, _, _ = self.request()
            drain_success += int(okay)
        with self.lock:
            average = (self.total_latency_us // self.successes
                       if self.successes else 0)
            during_apdu = 0
            apdu_duration_ms = 0
            if self.apdu_start is not None and self.apdu_end is not None:
                during_apdu = sum(
                    self.apdu_start <= started <= self.apdu_end
                    for started in self.success_start_times)
                apdu_duration_ms = (self.apdu_end - self.apdu_start) // 1_000_000
            print("USB_CONTROL:SUMMARY "
                  f"requests={self.requests} success={self.successes} "
                  f"fail={self.failures} during_apdu={during_apdu} "
                  f"apdu_duration_ms={apdu_duration_ms} "
                  f"avg_latency_us={average} max_latency_us={self.max_latency_us} "
                  f"drain={drain_success}/5", flush=True)
            return self.failures, during_apdu, drain_success, apdu_duration_ms


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial", required=True,
                        help="RP2350 unique USB serial/chip ID")
    parser.add_argument("--reader-name",
                        help="exact PC/SC reader name for the RSA probe")
    parser.add_argument("--preflight-seconds", type=int, default=0,
                        help="only flood descriptor requests for N seconds")
    args = parser.parse_args()
    if not args.serial or not args.serial.isascii():
        parser.error("--serial must be ASCII")
    if args.preflight_seconds < 0:
        parser.error("--preflight-seconds must not be negative")
    if args.preflight_seconds == 0 and not args.reader_name:
        parser.error("--reader-name is required for the RSA APDU test")

    require_host_usb_access()
    libusb, context, handle, location = open_target(args.serial)
    print(f"USB_CONTROL:TARGET vid={VID:04X} pid={PID:04X} "
          f"serial={args.serial} bus_address={location}", flush=True)
    traffic = ControlTraffic(libusb, handle)
    monitor_log = None
    monitor_process = None
    monitor_path = None
    test_status = 0
    apdu_seen = False
    apdu_finished = False
    traffic.start()

    if args.preflight_seconds:
        deadline = time.monotonic() + args.preflight_seconds
        while time.monotonic() < deadline:
            time.sleep(0.05)
        failures, _, drain, _ = traffic.stop_and_drain()
        libusb.libusb_close(handle)
        libusb.libusb_exit(context)
        return 0 if failures == 0 and drain == 5 else 1

    timestamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    log_dir = ROOT / "artifacts/logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    monitor_path = log_dir / f"ccid-progress-monitor-{args.serial}-{timestamp}.log"
    monitor_log = monitor_path.open("w", encoding="utf-8")
    env = os.environ.copy()
    env["PICO2_HOST_USB_ACCESS"] = "1"
    monitor_process = subprocess.Popen(
        [str(ROOT / "tools/monitor"), "--serial", args.serial,
         "--seconds", "240"],
        cwd=ROOT, env=env, stdout=monitor_log, stderr=subprocess.STDOUT,
        start_new_session=True)

    command = [str(ROOT / "tools/test-asymm-crypto-pcsc"), args.reader_name,
               "RSA 3072-bit generation and signature"]
    process = subprocess.Popen(command, cwd=ROOT, env=env,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, bufsize=1)
    print(f"PHYSICAL:START control_request=GET_DESCRIPTOR device={args.serial} "
          "rsa_vector='RSA 3072-bit generation and signature'", flush=True)
    try:
        assert process.stdout is not None
        for line in process.stdout:
            print(line, end="", flush=True)
            if "APDU:START label=RSA 3072-bit generation and signature" in line:
                traffic.mark_apdu(True)
                apdu_seen = True
            if "TEST:PASS RSA 3072-bit generation and signature" in line:
                traffic.mark_apdu(False)
                apdu_finished = True
        test_status = process.wait()
    finally:
        failures, during_apdu, drain_success, apdu_duration_ms = \
            traffic.stop_and_drain()
        time.sleep(2)
        if monitor_process is not None and monitor_process.poll() is None:
            os.killpg(monitor_process.pid, signal.SIGTERM)
            try:
                monitor_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(monitor_process.pid, signal.SIGKILL)
                monitor_process.wait()
        if monitor_log is not None:
            monitor_log.close()
        libusb.libusb_close(handle)
        libusb.libusb_exit(context)

    if monitor_path is not None:
        print(f"SERIAL:LOG {monitor_path.relative_to(ROOT)}", flush=True)
        for line in monitor_path.read_text(encoding="utf-8", errors="replace").splitlines():
            if "CHOUPI:USB_PROGRESS" in line:
                print(f"SERIAL:{line}", flush=True)
    print(f"PHYSICAL:RESULT exit={test_status} apdu_started={int(apdu_seen)} "
          f"apdu_passed={int(apdu_finished)} control_failures={failures} "
          f"control_during_apdu={during_apdu} apdu_duration_ms={apdu_duration_ms} "
          f"drain={drain_success}/5",
          flush=True)
    min_during_apdu = max(10, apdu_duration_ms // 200)
    return 0 if (test_status == 0 and apdu_seen and apdu_finished and
                 failures == 0 and during_apdu >= min_during_apdu and
                 drain_success == 5) else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f"PHYSICAL:FAIL {error}", file=sys.stderr, flush=True)
        raise
