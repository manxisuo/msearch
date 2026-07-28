# Example CMake toolchain for Kylin / Ubuntu aarch64 cross-build from x86_64.
#
# Prerequisites (host):
#   sudo apt-get install g++-aarch64-linux-gnu
#   # plus an aarch64 Qt 5.12.8 sysroot or multiarch Qt packages
#
# Usage:
#   cmake -S . -B build-aarch64 \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-kylin-toolchain.cmake \
#     -DCMAKE_PREFIX_PATH=/path/to/qt-5.12.8-aarch64
#   cmake --build build-aarch64 -j$(nproc)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Optional: point to a sysroot with target libraries
# set(CMAKE_SYSROOT /path/to/kylin-aarch64-sysroot)
# set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
