#!/bin/sh

COMPILERS="gcc clang"
MODES="Debug Release"

for COMPILER in $COMPILERS; do
  for MODE in $MODES; do
    BUILD_DIR="_build_"$COMPILER"_"$MODE

    if [ ! -d $BUILD_DIR ]; then
      echo "\\033[1m\\033[37m\\033[42m" Creating new build directory: $BUILD_DIR "\\033[0m"
      mkdir -p $BUILD_DIR
      cd $BUILD_DIR
      case $COMPILER in
        gcc) CXXCOMPILER=g++ ;;
        clang) CXXCOMPILER=clang++ ;;
        *) CXXCOMPILER=$COMPILER ;;
      esac
      CC=$COMPILER CXX=$CXXCOMPILER cmake -G Ninja -D CMAKE_BUILD_TYPE=$MODE ..
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

for MODULE in $( ls -1d */ | grep -v _build_ ); do
  echo "\\033[1m\\033[37m\\033[42m" Analyzing module $MODULE "\\033[0m"
  for SOURCE in $( find $MODULE"include" $MODULE"src" $MODULE"test" -regex "\(.*\.cpp\)\|\(.*\.h\)" ); do
    if clang-format -style=file -output-replacements-xml $SOURCE | grep "<replacement " >/dev/null; then
      echo "\\033[1m\\033[37m\\033[41m" Syntax convention problem with $SOURCE "\\033[0m"
      clang-format -style=file $SOURCE | diff $SOURCE -
      exit 3
    fi
  done
done

echo "\\033[1m\\033[37m\\033[42m Build successful. \\033[0m"
exit 0

