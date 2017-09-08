#ifndef ZAMT_CORE_TESTSUITE_H_
#define ZAMT_CORE_TESTSUITE_H_

/// Use this in tests
/**
 * Tests can use macros to check conditions
 * Failure exit code of the program is handled
 * Use TEST_BEGIN() and TEST_END() for the main function body
 */

#ifndef TEST
#error "TestSuite can only be used in testing mode"
#endif

#include <cstdio>
#include <cstdlib>

#define TEST_BEGIN() int main(int, char**) {
#define TEST_END()               \
  return zamt::g_test_exit_code; \
  }

/// Stops execution and breaks test on condition failure
#define ASSERT(condition)                                              \
  if (!(condition)) {                                                  \
    printf("ASSERT(%s) failed in %s at %s:%d\n", #condition, __func__, \
           __FILE__, __LINE__);                                        \
    abort();                                                           \
  }

/// Let execution go forward but breaks test on condition failure
#define EXPECT(condition)                                              \
  if (!(condition)) {                                                  \
    printf("EXPECT(%s) failed in %s at %s:%d\n", #condition, __func__, \
           __FILE__, __LINE__);                                        \
    zamt::g_test_exit_code = EXIT_FAILURE;                             \
  }

namespace zamt {

extern int g_test_exit_code;

}  // namespace zamt

#endif  // ZAMT_CORE_TESTSUITE_H_
