#ifndef ZAMT_CORE_MODULECENTER_H_
#define ZAMT_CORE_MODULECENTER_H_

/// Handles module life cycle and access
/**
 * This object ties all available (compiled in) modules to the system.
 * Not all modules need to be initialized (e.g. libraries).
 * Those who need initialization will have a singleton like object having
 * the same life cycle as this object.
 * These instances can be accessed through this object.
 * Initialization is done in two stages propagating ModuleCenter in 2nd stage.
 *
 * A module's presence can be detected by the symbol defined
 * ZAMT_MODULE_<uppercase module name>
 */

#include <cstddef>
#include <map>
#include "zamt/core/Module.h"

namespace zamt {

class ModuleCenter {
 public:
  /// Pass runtime configuration data to all modules.
  ModuleCenter(int argc, const char* const* argv);
  ~ModuleCenter();

  ModuleCenter(const ModuleCenter&) = delete;
  ModuleCenter(ModuleCenter&&) = delete;
  ModuleCenter& operator=(const ModuleCenter&) = delete;
  ModuleCenter& operator=(ModuleCenter&&) = delete;

  /// This is the general way to access a module's running instance
  template <class ModuleClass>
  ModuleClass& Get() const;

  /// Returns an ID guaranteed to be unique for the module type (not instance)
  template <class ModuleClass>
  static size_t GetId();

#ifdef TEST
  static int GetRegisteredModuleNumber() { return module_num_; }
#endif

 private:
  template <class ModuleClass>
  struct ModuleBootstrap {
    ModuleBootstrap();
    int dummy;
  };

  template <class ModuleClass>
  struct ModuleStub {
    static size_t GetId();
    static Module* Create(int argc, const char* const* argv);
    static void Init(const ModuleCenter* module_center, Module* module);
    static void Destroy(Module* instance);
    static ModuleBootstrap<ModuleClass> bootstrap_;
  };

  struct ModuleInitRecord {
    size_t key;
    Module* (*create_function)(int argc, const char* const* argv);
    void (*init_function)(const ModuleCenter*, Module*);
    void (*destroy_function)(Module*);
  };

  static const int kMaxModulesNum = 64;

  static int module_num_;
  static ModuleInitRecord module_inits_[kMaxModulesNum];

  std::map<size_t, Module*> module_instances_;
};

}  // namespace zamt

#include "zamt/core/ModuleCenter-impl.h"

#endif  // ZAMT_CORE_MODULECENTER_H_
