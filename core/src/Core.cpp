#include "zamt/core/Core.h"

#include "zamt/core/Log.h"
#include "zamt/core/Scheduler.h"

#include <signal.h>
#include <cassert>
#include <cstring>

namespace {

zamt::Core* g_core = nullptr;

void quit_signaled(int signal_number) {
  int exit_code = -1;
  if (signal_number == SIGTERM)
    exit_code = zamt::Core::kExitCodeSIGTERM;
  else if (signal_number == SIGINT)
    exit_code = zamt::Core::kExitCodeSIGINT;
  assert(g_core);
  g_core->Quit(exit_code);
}

void handle_signal(int signal_number) {
  struct sigaction signal_action;
  memset(&signal_action, 0, sizeof(struct sigaction));
  signal_action.sa_handler = quit_signaled;
  int er = sigaction(signal_number, &signal_action, nullptr);
  assert(er == 0);
  (void)er;
}

}  // namespace

namespace zamt {

const char* Core::kModuleLabel = "core";
const char* Core::kHelpParamStr = "-h";
const char* Core::kThreadsParamStr = "-j";

#ifdef TEST
void Core::ReInitExitCode() {
  exit_code_.store(Core::kNoExitCode, std::memory_order_release);
}
#endif

Core::Core(int argc, const char* const* argv) : cli_(argc, argv) {
  log_.reset(new Log(kModuleLabel, cli_));
  log_->LogMessage("Starting...");

  handle_signal(SIGTERM);
  handle_signal(SIGINT);
  // In case of multiple instances, it is enough to get only one
  g_core = this;

  if (cli_.HasParam(kHelpParamStr)) {
    PrintHelp();
    Log::PrintHelp4Verbose();
    Quit(kExitCodeHelp);
    return;
  }

  int workers = cli_.GetNumParam(kThreadsParamStr);
  if (workers == CLIParameters::kNotFound) workers = 0;
  log_->LogMessage("Launching scheduler...");
  scheduler_.reset(new Scheduler(workers));
  log_->LogMessage("Scheduler started with ", scheduler_->GetNumberOfWorkers(),
                   " threads.");
}

Core::~Core() { log_->LogMessage("Stopping..."); }

void Core::Initialize(const ModuleCenter* mc) { mc_ = mc; }

void Core::Quit(int exit_code) {
  log_->LogMessage("Shutdown initiated with exit code ", exit_code);
  for (const auto& quit_cb : on_quit_callbacks_) {
    quit_cb(exit_code);
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    exit_code_.store(exit_code, std::memory_order_release);
  }
  cond_var_.notify_all();
}

int Core::WaitForQuit() {
  log_->LogMessage("Ready, idling...");
  std::unique_lock<std::mutex> lock(mutex_);
  int exit_code = exit_code_.load(std::memory_order_acquire);
  while (exit_code == kNoExitCode) {
    // Loop can be used with wait_for for periodic health check and statistics
    cond_var_.wait(lock);
    exit_code = exit_code_.load(std::memory_order_acquire);
  }
  log_->LogMessage("Shutdown started with exit code ", exit_code);
  return exit_code;
}

void Core::RegisterForQuitEvent(OnQuitCallback on_quit_callback) {
  on_quit_callbacks_.push_back(on_quit_callback);
}

Scheduler& Core::scheduler() {
  assert(scheduler_);
  return *scheduler_;
}

void Core::PrintHelp() {
  Log::Print("ZAMT Core Module");
  Log::Print(" -h             Get help from all active modules and quit.");
  Log::Print(
      " -jNum          Set number of worker threads in scheduler."
      " 0 means autodetect.");
}

std::atomic<int> Core::exit_code_(Core::kNoExitCode);
std::mutex Core::mutex_;
std::condition_variable Core::cond_var_;

}  // namespace zamt
