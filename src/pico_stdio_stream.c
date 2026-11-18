// Route newlib fprintf/vfprintf to the Pico SDK printf implementation.
//
// The shared VM sources report diagnostics with std::fprintf(stderr, ...).
// On the host build stderr is a separate stream; on the Pico, stdout and
// stderr both end in the same Pico stdio sink. Calling newlib's fprintf
// linked its full vfprintf (float formatting, dtoa, multi-precision helpers,
// locale and FILE buffering), about 18 KB of flash, next to the SDK's own
// printf that is already linked. The linker wraps fprintf and vfprintf for
// the firmware only (see CMakeLists.txt), so the output text and sink are
// unchanged. No other FILE stream exists on the device; anything else fails
// closed.

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

int __wrap_vfprintf(FILE *stream, const char *format, va_list args) {
  if (stream != stdout && stream != stderr) {
    errno = EBADF;
    return -1;
  }
  return vprintf(format, args);
}

int __wrap_fprintf(FILE *stream, const char *format, ...) {
  va_list args;
  va_start(args, format);
  const int result = __wrap_vfprintf(stream, format, args);
  va_end(args);
  return result;
}
