#include "zamt/core/Core.h"

#include <signal.h>
#include <cassert>
#include <cstring>

namespace {

zamt::Core* g_core = nullptr;

void quit_signaled(int signal_number) {
  int exit_code = -1;
  if (signal_number == SIGTERM)
    exit_code = zamt::Core::kErrorCodeSIGTERM;
  else if (signal_number == SIGINT)
    exit_code = zamt::Core::kErrorCodeSIGINT;
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
}

namespace zamt {

Core::Core(int argc, const char* const* argv) : cli_(argc, argv) {
  handle_signal(SIGTERM);
  handle_signal(SIGINT);
  // In case of multiple instances, it is enough to get only one
  g_core = this;
}

Core::~Core() {}

void Core::Initialize(const ModuleCenter* mc) { mc_ = mc; }

void Core::Quit(int exit_code) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    exit_code_.store(exit_code, std::memory_order_release);
  }
  cond_var_.notify_all();
}

int Core::WaitForQuit() {
  std::unique_lock<std::mutex> lock(mutex_);
  int exit_code = exit_code_.load(std::memory_order_acquire);
  while (exit_code == kNoExitCode) {
    // Loop can be used with wait_for for periodic health check and statistics
    cond_var_.wait(lock);
    exit_code = exit_code_.load(std::memory_order_acquire);
  }
  return exit_code;
}

std::atomic<int> Core::exit_code_(Core::kNoExitCode);
std::mutex Core::mutex_;
std::condition_variable Core::cond_var_;

}  // namespace zamt
