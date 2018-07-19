cmake_minimum_required(VERSION 3.5)
project(zamt CXX)
enable_testing()

include(generalconf.cmake)

include(modules.cmake)

include(targets.cmake)

AddAllTests("${zamt_modules}")

