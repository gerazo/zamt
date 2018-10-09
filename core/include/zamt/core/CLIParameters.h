#ifndef ZAMT_CORE_CLIPARAMETERS_H_
#define ZAMT_CORE_CLIPARAMETERS_H_

/// Helper class for command line parameter handling

namespace zamt {

class CLIParameters {
 public:
  const static int kNotFound = -99999;

  CLIParameters(int argc, const char* const* argv);

  /// Returns true if the given parameter is present as is, without suffix.
  bool HasParam(const char* param) const;

  /// Finds the 1st parameter having this prefix and returns the rest,
  /// otherwise returns nullptr.
  const char* GetParam(const char* param_prefix) const;

  /// Finds the 1st parameter having this prefix and returns the rest
  /// converted to a number, otherwise returns kNotFound.
  int GetNumParam(const char* param_prefix) const;

  int argc() const { return argc_; }
  const char* const* argv() const { return argv_; }

 private:
  int argc_;
  const char* const* argv_;
};

}  // namespace zamt

#endif  // ZAMT_CORE_CLIPARAMETERS_H_
