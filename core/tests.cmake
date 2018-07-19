set(this_module core)


set(other_modules
)

set(test_cpps
  CLIParametersTest.cpp
)
AddTest(CLIParametersTest ${this_module} "${other_modules}" "${test_cpps}")

set(test_cpps
  ModuleCenterTest.cpp
)
AddTest(ModuleCenterTest ${this_module} "${other_modules}" "${test_cpps}")

set(test_cpps
  SchedulerTest.cpp
)
AddTest(SchedulerTest ${this_module} "${other_modules}" "${test_cpps}")

