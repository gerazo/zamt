#ifndef TEST

#include "zamt/core/Core.h"
#include "zamt/core/ModuleCenter.h"

int main(int argc, char** argv) {
  zamt::ModuleCenter mc(argc, argv);
  zamt::Core& core = mc.Get<zamt::Core>();
  (void)core;
  return 0;
}

#endif
