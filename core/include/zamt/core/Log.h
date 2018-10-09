#ifndef ZAMT_CORE_LOG_H_
#define ZAMT_CORE_LOG_H_

#include "zamt/core/CLIParameters.h"

/// Very simple handling of output to console.

namespace zamt {

class Log {
 public:
  const static char* kVerboseParamStr;

  /// Label is prefixed to every log message. Set verbose mode.
  Log(const char* label, const CLIParameters& cli);

  /// Unconditionally print message to console.
  static void Print(const char* messa);
  /// Print help for verbose handling.
  static void PrintHelp4Verbose();

  /// Log only if verbose mode is on, output message in nice log format.
  void LogMessage(const char* msg);
  void LogMessage(const char* msg, int num, const char* suffix = "");
  void LogMessage(const char* msg, float num, const char* suffix = "");

 private:
  const char* label_;
  bool verbose_;
};

}  // namespace zamt

#endif  // ZAMT_CORE_LOG_H_
