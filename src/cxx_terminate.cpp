// Replace libstdc++'s verbose terminate handler.
//
// The Arm GNU Toolchain's libstdc++ default terminate handler
// (__gnu_cxx::__verbose_terminate_handler) pulls in the C++ symbol demangler
// (cp-demangle.o), which is about 33 KB of flash that the JCVM image cannot
// spare. Defining the handler here makes the linker use this definition
// instead of extracting vterminate.o from libstdc++.a. Firmware never prints
// exception diagnostics through this path, so abort() is the intended action.
#include <cstdlib>

namespace __gnu_cxx {
void __verbose_terminate_handler() { std::abort(); }
} // namespace __gnu_cxx
