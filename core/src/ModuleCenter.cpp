#include "zamt/core/ModuleCenter.h"

#include <tuple>

namespace zamt {

ModuleCenter::ModuleCenter() {
  for (int i = 0; i < module_num_; ++i) {
    ModuleInitRecord& rec = module_inits_[i];
    Module* instance = (*rec.create_function)();
    bool inserted;
    std::tie(std::ignore, inserted) =
        module_instances_.emplace(rec.key, instance);
    assert(inserted);
  }
  for (int i = 0; i < module_num_; ++i) {
    ModuleInitRecord& rec = module_inits_[i];
    auto instanceIter = module_instances_.find(rec.key);
    assert(instanceIter != module_instances_.end());
    (*rec.init_function)(this, instanceIter->second);
  }
}

ModuleCenter::~ModuleCenter() {
  for (int i = 0; i < module_num_; ++i) {
    ModuleInitRecord& rec = module_inits_[i];
    auto instanceIter = module_instances_.find(rec.key);
    assert(instanceIter != module_instances_.end());
    (*rec.destroy_function)(instanceIter->second);
  }
}

int ModuleCenter::module_num_ = 0;

ModuleCenter::ModuleInitRecord
    ModuleCenter::module_inits_[ModuleCenter::kMaxModulesNum];

}  // namespace zamt
