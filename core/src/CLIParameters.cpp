#include "zamt/core/CLIParameters.h"

#include <cstring>

namespace zamt {

CLIParameters::CLIParameters(int argc, const char* const* argv) {
  argc_ = argc;
  argv_ = argv;
}

bool CLIParameters::HasParam(const char* param) const {
  for (int i = 1; i < argc_; ++i) {
    if (strcmp(argv_[i], param) == 0) {
      return true;
    }
  }
  return false;
}

const char* CLIParameters::GetParam(const char* param_prefix) const {
  for (int i = 1; i < argc_; ++i) {
    const char* found = strstr(argv_[i], param_prefix);
    if (found) {
      return found + strlen(param_prefix);
    }
  }
  return nullptr;
}

}  // namespace zamt
