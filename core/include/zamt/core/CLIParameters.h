#ifndef ZAMT_CORE_CLIPARAMETERS_H_
#define ZAMT_CORE_CLIPARAMETERS_H_

/// Helper class for command line parameter handling

namespace zamt {

class CLIParameters {
 public:
  CLIParameters(int argc, const char* const* argv);

  /// Returns true if having a parameter exactly as given.
  bool HasParam(const char* param) const;

  /// Finds the 1st parameter having this prefix and returns the rest,
  /// otherwise returns nullptr.
  const char* GetParam(const char* param_prefix) const;

 private:
  int argc_;
  const char* const* argv_;
};

}  // namespace zamt

#endif  // ZAMT_CORE_CLIPARAMETERS_H_
