#ifndef ZAMT_CORE_CORE_H_
#define ZAMT_CORE_CORE_H_

/// Core Module collects general stateful services

#include "zamt/core/CLIParameters.h"
#include "zamt/core/Module.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace zamt {

class Core : public Module {
 public:
  const static int kErrorCodeSIGTERM = 101;
  const static int kErrorCodeSIGINT = 102;

  Core(int argc, const char* const* argv);
  ~Core();

  void Initialize(const ModuleCenter* mc);

  /// Initiate normal shutdown of the system.
  void Quit(int exit_code);

  /// Blocks execution until someone calls Quit(), returns the exit code.
  int WaitForQuit();

 private:
  const static int kNoExitCode = -999999;

  // These are system wide and shut every instance down in the current process.
  static std::atomic<int> exit_code_;
  static std::mutex mutex_;
  static std::condition_variable cond_var_;

  CLIParameters cli_;
  const ModuleCenter* mc_ = nullptr;
};

}  // namespace zamt

#endif  // ZAMT_CORE_CORE_H_
