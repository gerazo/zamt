#include "zamt/core/ModuleCenter.h"
#include "zamt/core/TestSuite.h"

using namespace zamt;

class ModuleOne : public Module {
 public:
  ModuleOne() {
    count++;
    data = 1;
    mcenter = nullptr;
  }
  ~ModuleOne() { count--; }
  void Initialize(const ModuleCenter* mc) { mcenter = mc; }

  static int count;
  int data;
  const ModuleCenter* mcenter;
};

class ModuleTwo : public Module {
 public:
  ModuleTwo() {
    count++;
    data = 2;
    mcenter = nullptr;
  }
  ~ModuleTwo() { count--; }
  void Initialize(const ModuleCenter* mc) { mcenter = mc; }

  static int count;
  int data;
  const ModuleCenter* mcenter;
};

int ModuleOne::count = 0;
int ModuleTwo::count = 0;

void RegisteredModuleNumberIsCorrect() {
  ASSERT(ModuleCenter::GetRegisteredModuleNumber() == 2);
}

void ModuleIdsAreUnique() {
  ASSERT(ModuleCenter::GetId<ModuleOne>() != ModuleCenter::GetId<ModuleTwo>());
}

void AllModulesAreStartedAndStopped() {
  EXPECT(ModuleOne::count == 0);
  EXPECT(ModuleTwo::count == 0);
  {
    ModuleCenter mc;
    EXPECT(ModuleOne::count == 1);
    EXPECT(ModuleTwo::count == 1);
    EXPECT(mc.Get<ModuleOne>().data == 1);
    EXPECT(mc.Get<ModuleTwo>().data == 2);
    EXPECT(mc.Get<ModuleOne>().mcenter == &mc);
    EXPECT(mc.Get<ModuleTwo>().mcenter == &mc);
  }
  EXPECT(ModuleOne::count == 0);
  EXPECT(ModuleTwo::count == 0);
}

void MultipleModulesCanLiveTogether() {
  EXPECT(ModuleOne::count == 0);
  EXPECT(ModuleTwo::count == 0);
  {
    ModuleCenter mc;
    EXPECT(ModuleOne::count == 1);
    EXPECT(ModuleTwo::count == 1);
    mc.Get<ModuleOne>().data = 3;
    mc.Get<ModuleTwo>().data = 4;
    EXPECT(mc.Get<ModuleOne>().mcenter == &mc);
    EXPECT(mc.Get<ModuleTwo>().mcenter == &mc);
    {
      ModuleCenter mc2;
      EXPECT(ModuleOne::count == 2);
      EXPECT(ModuleTwo::count == 2);
      EXPECT(mc2.Get<ModuleOne>().data == 1);
      EXPECT(mc2.Get<ModuleTwo>().data == 2);
      EXPECT(mc2.Get<ModuleOne>().mcenter == &mc2);
      EXPECT(mc2.Get<ModuleTwo>().mcenter == &mc2);
    }
    EXPECT(ModuleOne::count == 1);
    EXPECT(ModuleTwo::count == 1);
    EXPECT(mc.Get<ModuleOne>().data == 3);
    EXPECT(mc.Get<ModuleTwo>().data == 4);
    EXPECT(mc.Get<ModuleOne>().mcenter == &mc);
    EXPECT(mc.Get<ModuleTwo>().mcenter == &mc);
  }
  EXPECT(ModuleOne::count == 0);
  EXPECT(ModuleTwo::count == 0);
}

TEST_BEGIN() {
  RegisteredModuleNumberIsCorrect();
  ModuleIdsAreUnique();
  AllModulesAreStartedAndStopped();
  MultipleModulesCanLiveTogether();
}
TEST_END()
