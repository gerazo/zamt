#ifndef ZAMT_CORE_CORE_H_
#define ZAMT_CORE_CORE_H_

/// Core Module collects general stateful services

#include "zamt/core/Module.h"

namespace zamt {

class Core : public Module {
 public:
  Core(int argc, const char* const* argv);
  ~Core();
  void Initialize(const ModuleCenter*);

 private:
};

}  // namespace zamt

#endif  // ZAMT_CORE_CORE_H_
