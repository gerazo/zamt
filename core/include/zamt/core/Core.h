#ifndef ZAMT_CORE_CORE_H_
#define ZAMT_CORE_CORE_H_

/// Core Module collects general stateful services

#include "zamt/core/CLIParameters.h"
#include "zamt/core/Log.h"
#include "zamt/core/Module.h"
#include "zamt/core/Scheduler.h"

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace zamt {

class Core : public Module {
 public:
  const static int kExitCodeHelp = 100;
  const static int kExitCodeSIGTERM = 101;
  const static int kExitCodeSIGINT = 102;

  const static char* kModuleLabel;
  const static char* kHelpParamStr;
  const static char* kThreadsParamStr;

#ifdef TEST
  /// For testing purposes, simulate if the process only starts now
  static void ReInitExitCode();
#endif

  Core(int argc, const char* const* argv);
  ~Core();

  void Initialize(const ModuleCenter* mc);

  /// Initiate normal shutdown of the system.
  void Quit(int exit_code);
  /// Blocks execution until someone calls Quit(), returns the exit code.
  int WaitForQuit();

  /// Get CLIParameters
  CLIParameters& cli() { return cli_; }
  /// Get the main Scheduler working in the system
  Scheduler& scheduler();

 private:
  const static int kNoExitCode = -999999;

  void PrintHelp();

  // These are system wide and shut every instance down in the current process.
  static std::atomic<int> exit_code_;
  static std::mutex mutex_;
  static std::condition_variable cond_var_;

  const ModuleCenter* mc_ = nullptr;
  Log* log_ = nullptr;
  CLIParameters cli_;
  Scheduler* scheduler_ = nullptr;
};

}  // namespace zamt

#endif  // ZAMT_CORE_CORE_H_
