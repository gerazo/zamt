#include "zamt/core/Core.h"
#include "zamt/core/ModuleCenter.h"
#include "zamt/core/TestSuite.h"

#include <signal.h>
#include <unistd.h>
#include <thread>

using namespace zamt;

const char* params[] = {"exec"};

void CallQuit(Core* core) {
  for (int i = 0; i < 19; ++i) std::this_thread::yield();
  core->Quit(99);
}

void ShutsDownFromOtherThread() {
  ModuleCenter mc(sizeof(params) / sizeof(char*), params);
  Core& core = mc.Get<zamt::Core>();
  core.ReInitExitCode();
  std::thread thr(CallQuit, &core);
  EXPECT(core.WaitForQuit() == 99);
  thr.join();
}

void CallQuitAtOnce(Core* core) { core->Quit(98); }

void ShutsDownFromOtherThreadImmediately() {
  ModuleCenter mc(sizeof(params) / sizeof(char*), params);
  Core& core = mc.Get<zamt::Core>();
  core.ReInitExitCode();
  std::thread thr(CallQuitAtOnce, &core);
  EXPECT(core.WaitForQuit() == 98);
  thr.join();
}

void Signal(int signal_number) {
  for (int i = 0; i < 19; ++i) std::this_thread::yield();
  int er = kill(getpid(), signal_number);
  ASSERT(er == 0);
}

void ShutsDownForSignal(int signal_number) {
  ModuleCenter mc(sizeof(params) / sizeof(char*), params);
  Core& core = mc.Get<zamt::Core>();
  core.ReInitExitCode();
  std::thread thr(Signal, signal_number);
  int expected = (signal_number == SIGTERM) ? Core::kErrorCodeSIGTERM
                                            : Core::kErrorCodeSIGINT;
  EXPECT(core.WaitForQuit() == expected);
  thr.join();
}

TEST_BEGIN() {
  ShutsDownFromOtherThread();
  ShutsDownFromOtherThreadImmediately();
  ShutsDownForSignal(SIGINT);
  ShutsDownForSignal(SIGTERM);
}
TEST_END()
