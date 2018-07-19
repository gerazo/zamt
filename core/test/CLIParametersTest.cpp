#include "zamt/core/CLIParameters.h"
#include "zamt/core/TestSuite.h"

#include <cstring>

using namespace zamt;

void WorksOnEmptyList() {
  const char* params[] = {"exec"};
  CLIParameters clip(sizeof(params) / sizeof(char*), params);
  EXPECT(!clip.HasParam("-h"));
  EXPECT(!clip.GetParam("-o"));
}

void FindsParam() {
  const char* params[] = {"exec", "-h", "-v"};
  CLIParameters clip(sizeof(params) / sizeof(char*), params);
  EXPECT(clip.HasParam("-h"));
  EXPECT(clip.HasParam("-v"));
  EXPECT(!clip.HasParam("-q"));
}

void ReturnsParamCorrectly() {
  const char* params[] = {"exec", "-oKutya", "-v"};
  CLIParameters clip(sizeof(params) / sizeof(char*), params);
  EXPECT(strcmp(clip.GetParam("-o"), "Kutya") == 0);
  EXPECT(strcmp(clip.GetParam("-v"), "") == 0);
  EXPECT(clip.GetParam("-q") == nullptr);
}

TEST_BEGIN() {
  WorksOnEmptyList();
  FindsParam();
  ReturnsParamCorrectly();
}
TEST_END()
