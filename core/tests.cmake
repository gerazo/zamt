set(this_module core)


set(test_cpps
  ModuleCenterTest.cpp
)
set(other_modules
)
AddTest(ModuleCenterTest ${this_module} "${other_modules}" "${test_cpps}")

set(test_cpps
  SchedulerTest.cpp
)
set(other_modules
)
AddTest(SchedulerTest ${this_module} "${other_modules}" "${test_cpps}")

