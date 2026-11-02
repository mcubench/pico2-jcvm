# Native progress callback contract

The Pico native crypto progress chain is:

1. `choupi_native_crypto_progress()` refreshes the VM's native-operation
   deadline, services the watchdog, then asks CCID to maintain USB progress.
2. `choupi_ccid_native_crypto_progress()` services at most eight queued USB
   events without waiting and may queue one CCID time-extension response when
   the active APDU has run for at least one second.
3. TinyUSB's CCID transfer callback may assemble and queue a received message.
   It must not execute the VM, process APDUs, or call the main CCID task.
4. `choupi_ccid_task()` is the only CCID entry point that dispatches a queued
   message into card management or the applet VM. It runs in the main loop.

The progress hook is called by the native random-byte callback used during
long asymmetric key generation. Its own work is bounded by a fixed event
count: update the execution timestamp, service the watchdog once, process at
most eight TinyUSB queue events without waiting, and optionally submit one
10-byte CCID time extension. Remaining events stay queued for a later call. It
must not wait for USB completion, poll for APDU input, call
`choupi_ccid_task()`, or execute an applet. The eight-event limit bounds the
number of callbacks dispatched by one progress invocation, including under a
sustained USB control-event stream. It does not by itself bound the execution
time of an individual callback; the CCID callbacks used here only update
transport state or queue APDU work.

Two reentry guards enforce the dispatch boundary. The CCID progress guard
prevents a nested progress callback from pumping TinyUSB recursively. The CCID
task guard prevents nested dispatch while `process_message()` is running. USB
callbacks may still queue a message during crypto work; the main loop processes
that message after the current APDU returns. Final response handling waits at
most 100 ms for an already submitted time-extension transfer before submitting
the APDU result. Each USB service pass in that wait has the same eight-event
limit.

On the PC backend the Pico watchdog/USB progress callback is not active. On
2026-10-31, Pico 2 `2BED26ED20556535` ran the ARM `max_crypto` image built from
the current source and completed RSA-3072 key generation and signature in a
single 49,184 ms PC/SC APDU, returning `019000`. The 15-second watchdog was
therefore serviced through the native random-byte progress hook, and the long
CCID operation reached its final response without a host transport timeout.
The subsequent finite serial monitor reported `TEST:SUMMARY pass=1 fail=0`.

On 2026-11-02, added post-operation counters and repeated a physical RSA-3072
generation/signature APDU on the same chip while the host sent USB device-
descriptor control requests every 20 ms. The APDU returned `019000` in
118,055 ms. Of 5,608 successful host requests, 4,336 began during the
118,267-ms APDU interval; zero failed and five more passed after it. Firmware
measured 5,795 progress calls, 3,301 us cumulative and 117 us maximum per
call. Its 169 USB service passes took 1,006 us cumulative and 94 us maximum,
processed 86 TinyUSB events total with a maximum of two in any pass, and
completed all 85 submitted CCID time extensions. There was no watchdog
recovery marker, `TEST:FAIL`, or `FAULT`. The final response arrived and the
temporary CAP cleanup passed. Logs, image hash, and the test wrapper are in
`.agent/HANDOFF.md` and the 2026-11-02 progress entry.
