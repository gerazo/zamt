#ifndef TEST

#include "zamt/core/Core.h"
#include "zamt/core/ModuleCenter.h"

int main(int argc, char** argv) {
  zamt::ModuleCenter mc(argc, argv);
  zamt::Core& core = mc.Get<zamt::Core>();
  return core.WaitForQuit();
}

#endif
