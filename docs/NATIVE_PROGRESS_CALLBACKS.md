# Native progress callback contract

The Pico native crypto progress chain is:

1. `choupi_native_crypto_progress()` refreshes the VM's native-operation
   deadline, services the watchdog, then asks CCID to maintain USB progress.
2. `choupi_ccid_native_crypto_progress()` may call `tud_task()` to service
   pending USB events and may queue one CCID time-extension response when the
   active APDU has run for at least one second.
3. TinyUSB's CCID transfer callback may assemble and queue a received message.
   It must not execute the VM, process APDUs, or call the main CCID task.
4. `choupi_ccid_task()` is the only CCID entry point that dispatches a queued
   message into card management or the applet VM. It runs in the main loop.

The progress hook is called by the native random-byte callback used during
long asymmetric key generation. Its own work is fixed per invocation: update
the execution timestamp, service the watchdog once, call TinyUSB once, and
optionally submit one 10-byte CCID time extension. It must not wait for USB
completion, poll for APDU input, call `choupi_ccid_task()`, or execute an
applet. TinyUSB's `tud_task()` drains its event queue until empty rather than
processing a fixed event count. During an active CCID command the bulk OUT
endpoint is not rearmed until response completion, so compliant CCID traffic
cannot continuously replenish that queue. A hostile stream of USB control
events can extend `tud_task()` work; this contract does not claim a strict
worst-case execution time under such a stream.

Two reentry guards enforce the dispatch boundary. The CCID progress guard
prevents a nested progress callback from pumping TinyUSB recursively. The CCID
task guard prevents nested dispatch while `process_message()` is running. USB
callbacks may still queue a message during crypto work; the main loop processes
that message after the current APDU returns. Final response handling waits at
most 100 ms for an already submitted time-extension transfer before submitting
the APDU result.

On the PC backend the Pico watchdog/USB progress callback is not active. On
2026-10-31, Pico 2 `2BED26ED20556535` ran the ARM `max_crypto` image built from
the current source and completed RSA-3072 key generation and signature in a
single 49,184 ms PC/SC APDU, returning `019000`. The 15-second watchdog was
therefore serviced through the native random-byte progress hook, and the long
CCID operation reached its final response without a host transport timeout.
The subsequent finite serial monitor reported `TEST:SUMMARY pass=1 fail=0`.
Firmware diagnostics do not expose a count of individual CCID extension
frames; the physical check verifies long-call completion rather than directly
counting those frames. See `.agent/HANDOFF.md` for image and log hashes.
