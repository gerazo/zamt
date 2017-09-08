#!/bin/sh

if command -v apt >/dev/null ; then
  # for the build
  sudo apt install cmake ninja-build binutils g++ llvm-dev clang clang-format clang-tidy

  # for audio module
  sudo apt install libpulse-dev
  # for gui module
  sudo apt install libgtkmm-3.0-dev
else
  # TODO write commands for other packaging systems
  echo This packaging system is not yet supported.
fi

