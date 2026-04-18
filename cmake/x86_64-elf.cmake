# 裸机 x86_64 ELF 交叉工具链
# 使用: cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-elf.cmake
#
# 若 PATH 里没有 x86_64-elf-gcc，可任选其一：
#   export CNOS_X86_64_ELF_GCC=/usr/local/cross/bin/x86_64-elf-gcc
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=... -DCMAKE_C_COMPILER=/path/to/x86_64-elf-gcc

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT CMAKE_C_COMPILER)
    if(DEFINED ENV{CNOS_X86_64_ELF_GCC} AND NOT "$ENV{CNOS_X86_64_ELF_GCC}" STREQUAL "")
        set(CMAKE_C_COMPILER "$ENV{CNOS_X86_64_ELF_GCC}")
    else()
        find_program(CMAKE_C_COMPILER NAMES x86_64-elf-gcc
            HINTS
                /usr/local/cross/bin
                /opt/cross/bin
                "$ENV{HOME}/opt/cross/bin"
                "$ENV{HOME}/cross/bin"
                "$ENV{HOME}/local/cross/bin"
                /usr/local/bin
            REQUIRED
        )
    endif()
endif()

if(NOT CMAKE_ASM_NASM_COMPILER)
    if(DEFINED ENV{CNOS_NASM} AND NOT "$ENV{CNOS_NASM}" STREQUAL "")
        set(CMAKE_ASM_NASM_COMPILER "$ENV{CNOS_NASM}")
    else()
        find_program(CMAKE_ASM_NASM_COMPILER NAMES nasm REQUIRED)
    endif()
endif()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
