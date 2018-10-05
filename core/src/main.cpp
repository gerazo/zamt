#ifndef TEST

#include "zamt/core/Core.h"
#include "zamt/core/ModuleCenter.h"

int main(int argc, char** argv) {
  zamt::ModuleCenter mc(argc, argv);
  zamt::Core& core = mc.Get<zamt::Core>();
  return core.WaitForQuit();
}

// Until libsigc++ has a bug causing unclean sigc::mem_fun deletion
// https://github.com/libsigcplusplus/libsigcplusplus/issues/10
extern "C" const char* __asan_default_options() {
  return "new_delete_type_mismatch=0";
}

#endif
