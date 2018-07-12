#!/bin/sh

COMPILERS="gcc clang"
MODES="Debug Release"
#MODES="Debug Release RelWithDebInfo"
CMAKE_PROJECT="Ninja"
#CMAKE_PROJECT="Eclipse CDT4 - Ninja"

BUILD_DEPS="cmake ninja-build binutils g++ llvm-dev clang clang-format"
PULSEAUDIO_DEPS="libpulse-dev"
GTKMM_DEPS="libgtkmm-3.0-dev"

ALL_DEPS="$BUILD_DEPS $PULSEAUDIO_DEPS $GTKMM_DEPS"


while [ $# -gt 0 ]; do
  case "$1" in
    -nodep) NODEP=1 ;;
    -noanal) NOANAL=1 ;;
    -compilers) shift; COMPILERS="$1" ;;
    *) echo "ZAMT build script parameters:"
       echo "  -nodep                       Skip dependency detection and installation."
       echo "  -noanal                      Skip extra analysis of sources."
       echo "  -compilers <compiler_list>   Tests with the given compiler toolchains."
       exit 99
       ;;
  esac
  shift
done

if [ "$NODEP" != "1" ]; then
  if command -v apt >/dev/null && command -v dpkg >/dev/null ; then
    if ! dpkg -s $ALL_DEPS >/dev/null; then
      echo "\\033[1m\\033[37m\\033[42m--- Installing Dependencies (needs sudo privileges)...\\033[0m"
      sudo apt install $ALL_DEPS
    fi
  else
    # TODO write commands for other packaging systems
    echo "\\033[1m\\033[37m\\033[41m" This packaging system is not yet supported!!! "\\033[0m" These dependencies should be installed: $ALL_DEPS
  fi
fi

for COMPILER in $COMPILERS; do
  for MODE in $MODES; do
    BUILD_DIR="_build_"$COMPILER"_"$MODE

    if [ ! -d $BUILD_DIR ]; then
      echo "\\033[1m\\033[37m\\033[42m" Creating new build setup: $BUILD_DIR "\\033[0m"
      mkdir -p $BUILD_DIR
      cd $BUILD_DIR
      CXXCOMPILER=$( echo $COMPILER | sed "s/gcc/g++/g" | sed "s/clang/clang++/g" )
      CC=$COMPILER CXX=$CXXCOMPILER cmake -G "$CMAKE_PROJECT" -DCMAKE_BUILD_TYPE=$MODE -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
      cd ..
    fi

    echo "\\033[1m\\033[37m\\033[42m" Compiling: $BUILD_DIR "\\033[0m"
    cd $BUILD_DIR
    if ninja ; then
      echo "\\033[1m\\033[37m\\033[42m" Testing: $BUILD_DIR "\\033[0m"
      if ninja test ; then
        echo "\\033[1m\\033[37m\\033[42m" Finished $BUILD_DIR "\\033[0m"
      else
        echo "\\033[1m\\033[37m\\033[41m" Testing of $BUILD_DIR failed."\\033[0m"
        cd ..
        exit 2
      fi
    else
      echo "\\033[1m\\033[37m\\033[41m" Compilation of $BUILD_DIR failed."\\033[0m"
      cd ..
      exit 1
    fi
    cd ..

  done
done

FAIL=0
if [ "$NOANAL" != "1" ]; then
  for MODULE in $( ls -1d */ | grep -v _build_ ); do
    echo "\\033[1m\\033[37m\\033[42m" Analyzing module $MODULE "\\033[0m"
    for SOURCE in $( find $MODULE"include" $MODULE"src" $MODULE"test" -regex "\(.*\.cpp\)\|\(.*\.h\)" ); do
      if clang-format -style=file -output-replacements-xml $SOURCE | grep "<replacement " >/dev/null; then
        echo "\\033[1m\\033[37m\\033[41m" Syntax convention problem with $SOURCE "\\033[0m"
        clang-format -style=file $SOURCE | diff $SOURCE -
        FAIL=3
      fi
    done
  done
fi

if [ "$FAIL" = "0" ]; then
  echo "\\033[1m\\033[37m\\033[42m Build successful. \\033[0m"
else
  echo "\\033[1m\\033[37m\\033[41m Build failed with code: $FAIL \\033[0m"
fi
exit $FAIL

