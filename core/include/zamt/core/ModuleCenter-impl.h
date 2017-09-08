#ifndef ZAMT_CORE_MODULECENTER_IMPL_H_
#define ZAMT_CORE_MODULECENTER_IMPL_H_

#include "zamt/core/ModuleCenter.h"

#include <cassert>

namespace zamt {

template <class ModuleClass>
ModuleClass& ModuleCenter::Get() const {
  size_t key = ModuleStub<ModuleClass>::GetId();
  auto instanceIter = module_instances_.find(key);
  assert(instanceIter != module_instances_.end());
  return *static_cast<ModuleClass*>(instanceIter->second);
}

template <class ModuleClass>
size_t ModuleCenter::GetId() {
  return ModuleStub<ModuleClass>::GetId();
}

template <class ModuleClass>
ModuleCenter::ModuleBootstrap<ModuleClass>::ModuleBootstrap() : dummy(0) {
  assert(module_num_ < kMaxModulesNum);
  ModuleInitRecord& rec = module_inits_[module_num_];
  rec.key = ModuleStub<ModuleClass>::GetId();
  rec.create_function = &ModuleStub<ModuleClass>::Create;
  rec.init_function = &ModuleStub<ModuleClass>::Init;
  rec.destroy_function = &ModuleStub<ModuleClass>::Destroy;
  ++module_num_;
}

template <class ModuleClass>
size_t ModuleCenter::ModuleStub<ModuleClass>::GetId() {
  return (size_t)&bootstrap_;
}

template <class ModuleClass>
Module* ModuleCenter::ModuleStub<ModuleClass>::Create() {
  return new ModuleClass();
}

template <class ModuleClass>
void ModuleCenter::ModuleStub<ModuleClass>::Init(
    const ModuleCenter* module_center, Module* module) {
  static_cast<ModuleClass*>(module)->Initialize(module_center);
}

template <class ModuleClass>
void ModuleCenter::ModuleStub<ModuleClass>::Destroy(Module* instance) {
  delete static_cast<ModuleClass*>(instance);
}

template <class ModuleClass>
ModuleCenter::ModuleBootstrap<ModuleClass>
    ModuleCenter::ModuleStub<ModuleClass>::bootstrap_;

}  // namesapce zamt

#endif  // ZAMT_CORE_MODULECENTER_IMPL_H_
