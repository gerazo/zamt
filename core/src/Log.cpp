#include "zamt/core/Log.h"

#include <cassert>
#include <cstdio>
#include <cstring>

namespace zamt {

const char* Log::kVerboseParamStr = "-v";

Log::Log(const char* label, const CLIParameters& cli) {
  const int kParamBufLength = 32;
  label_ = label;
  if (cli.HasParam(kVerboseParamStr)) {
    verbose_ = true;
    return;
  }
  assert(strlen(label) < kParamBufLength - 3);
  char str[kParamBufLength];
  strcpy(str, kVerboseParamStr);
  strcat(str, label);
  if (cli.GetParam(str) != nullptr) {
    verbose_ = true;
    return;
  }
  verbose_ = false;
}

void Log::Print(const char* messa) { printf("%s\n", messa); }

void Log::PrintHelp4Verbose() {
  Print(" -v             Set verbose status information mode globally.");
  Print(" -vModuleName   Set verbose mode only in ModuleName.");
}

void Log::LogMessage(const char* msg) {
  if (verbose_) {
    printf("[%s] %s\n", label_, msg);
  }
}

void Log::LogMessage(const char* msg, int num, const char* suffix) {
  if (verbose_) {
    printf("[%s] %s%d%s\n", label_, msg, num, suffix);
  }
}

void Log::LogMessage(const char* msg, float num, const char* suffix) {
  if (verbose_) {
    printf("[%s] %s%f%s\n", label_, msg, (double)num, suffix);
  }
}

}  // namespace zamt
