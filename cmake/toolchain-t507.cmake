# ---------------------------------------------------------------------------
# toolchain-t507.cmake
#
# CMake toolchain file for Allwinner T507 (Cortex-A53 × 4, aarch64)
# running Timesys Linux.
#
# USAGE
#   1. Source the Timesys SDK environment-setup script BEFORE configuring,
#      OR set TIMESYS_SDK_ROOT to your SDK sysroot path:
#
#        export TIMESYS_SDK_ROOT=/opt/timesys/t507/aarch64-linux-gnu
#
#   2. Configure with the preset:
#        cmake --preset t507-arm64
#
#   OR manually:
#        cmake -B build/t507 \
#              -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-t507.cmake \
#              -DCMAKE_BUILD_TYPE=Release
# ---------------------------------------------------------------------------

set(CMAKE_SYSTEM_NAME    Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# --------------------------------------------------------------------------
# Toolchain prefix — adjust if your cross-compiler has a different prefix
# --------------------------------------------------------------------------
set(CROSS_COMPILE_PREFIX "aarch64-linux-gnu-")

# Allow override from environment
if(DEFINED ENV{CROSS_COMPILE})
    set(CROSS_COMPILE_PREFIX "$ENV{CROSS_COMPILE}")
endif()

find_program(CMAKE_C_COMPILER   ${CROSS_COMPILE_PREFIX}gcc  REQUIRED)
find_program(CMAKE_CXX_COMPILER ${CROSS_COMPILE_PREFIX}g++  REQUIRED)
find_program(CMAKE_AR           ${CROSS_COMPILE_PREFIX}ar   REQUIRED)
find_program(CMAKE_STRIP        ${CROSS_COMPILE_PREFIX}strip)

# --------------------------------------------------------------------------
# Sysroot
# --------------------------------------------------------------------------
if(DEFINED ENV{TIMESYS_SDK_ROOT})
    set(CMAKE_SYSROOT "$ENV{TIMESYS_SDK_ROOT}")
elseif(DEFINED ENV{OECORE_TARGET_SYSROOT})
    # Timesys / Yocto SDK exports this
    set(CMAKE_SYSROOT "$ENV{OECORE_TARGET_SYSROOT}")
endif()

if(CMAKE_SYSROOT)
    message(STATUS "Toolchain sysroot: ${CMAKE_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
endif()

# --------------------------------------------------------------------------
# CPU flags for Cortex-A53
# --------------------------------------------------------------------------
set(T507_CPU_FLAGS "-mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard")
# Note: for aarch64 the FPU flags are different from arm32:
set(T507_CPU_FLAGS "-mcpu=cortex-a53")

set(CMAKE_C_FLAGS_INIT   "${T507_CPU_FLAGS} -pipe")
set(CMAKE_CXX_FLAGS_INIT "${T507_CPU_FLAGS} -pipe")

set(CMAKE_C_FLAGS_RELEASE   "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
