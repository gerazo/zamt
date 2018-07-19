#ifndef ZAMT_CORE_MODULE_H_
#define ZAMT_CORE_MODULE_H_

/// Interface of a stateful module started and stopped by ModuleCenter

namespace zamt {

class ModuleCenter;

class Module {
 public:
  /// Constructor in descendants is used to pass runtime configuration data.
  Module(/*int argc, const char* const* argv*/) {}
  ~Module() {}
  void Initialize(const ModuleCenter*) {}

  Module(const Module&) = delete;
  Module(Module&&) = delete;
  Module& operator=(const Module&) = delete;
  Module& operator=(Module&&) = delete;
};

}  // namespace zamt

#endif  // ZAMT_CORE_MODULE_H_
