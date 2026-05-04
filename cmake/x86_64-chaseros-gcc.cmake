# ChaserOS native-target GCC toolchain (bootstrap stage).
#
# Requires a toolchain that provides:
#   x86_64-chaseros-gcc / g++ / ld / ar / objcopy
#
# Suggested:
#   source <prefix>/bin/chaseros-native-gcc-env.sh

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CHASEROS_NATIVE_TARGET "x86_64-chaseros" CACHE STRING "ChaserOS target triple")
set(CHASEROS_SYSROOT "$ENV{CHASEROS_SYSROOT}" CACHE PATH "ChaserOS sysroot")

set(_trip "${CHASEROS_NATIVE_TARGET}")

if(NOT CHASEROS_SYSROOT)
  set(CHASEROS_SYSROOT "/opt/chaseros/${_trip}-sysroot")
endif()

set(CMAKE_C_COMPILER   "${_trip}-gcc")
set(CMAKE_CXX_COMPILER "${_trip}-g++")
set(CMAKE_ASM_COMPILER "${_trip}-gcc")
set(CMAKE_AR           "${_trip}-ar")
set(CMAKE_RANLIB       "${_trip}-ranlib")
set(CMAKE_LINKER       "${_trip}-ld")
set(CMAKE_OBJCOPY      "${_trip}-objcopy")

set(CMAKE_SYSROOT "${CHASEROS_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${CHASEROS_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS_INIT "-ffreestanding -fno-stack-protector")
set(CMAKE_CXX_FLAGS_INIT "-ffreestanding -fno-stack-protector -fno-exceptions -fno-rtti")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostdlib")
