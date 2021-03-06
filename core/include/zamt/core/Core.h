#ifndef ZAMT_CORE_CORE_H_
#define ZAMT_CORE_CORE_H_

/// Core Module collects general stateful services

#include "zamt/core/CLIParameters.h"
#include "zamt/core/Module.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

namespace zamt {

class Log;
class Scheduler;

class Core : public Module {
 public:
  using OnQuitCallback = std::function<void(int exit_code)>;

  const static int kExitCodeHelp = 100;
  const static int kExitCodeSIGTERM = 101;
  const static int kExitCodeSIGINT = 102;
  const static int kExitCodeAudioProblem = 200;

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
  /// The given function is called immediately when quit is called before
  /// core thread starts the shutdown process. Objects should not rely on
  /// other objects existence after this point.
  void RegisterForQuitEvent(OnQuitCallback on_quit_callback);

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
  std::unique_ptr<Log> log_;
  CLIParameters cli_;
  std::unique_ptr<Scheduler> scheduler_;
  std::deque<OnQuitCallback> on_quit_callbacks_;
};

}  // namespace zamt

#endif  // ZAMT_CORE_CORE_H_
