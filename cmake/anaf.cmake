# ANAF 裸机交叉工具链（与 cmake/x86_64-elf.cmake 同类，名称与 env 约定不同）
#
# ANAF_GNU_TARGET 默认 x86_64-elf；若工具链安装到其他三元组，构建前 export 该变量。
#
# 使用:
#   source /path/to/prefix/anaf-toolchain.env   # 可选，设置 PATH 与 ANAF_PREFIX
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/anaf.cmake
#
# 或直接指定编译器:
#   export ANAF_GCC=/path/to/x86_64-elf-gcc
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/anaf.cmake

set(CMAKE_SYSTEM_NAME Generic)

if(DEFINED ENV{ANAF_GNU_TARGET} AND NOT "$ENV{ANAF_GNU_TARGET}" STREQUAL "")
    set(ANAF_GNU_TARGET "$ENV{ANAF_GNU_TARGET}")
else()
    set(ANAF_GNU_TARGET "x86_64-elf")
endif()

# 与裸机 x86_64 内核一致时处理器名仍为 x86_64（勿把整个三元组塞进 PROCESSOR）
if(ANAF_GNU_TARGET MATCHES "^x86_64")
    set(CMAKE_SYSTEM_PROCESSOR x86_64)
else()
    set(CMAKE_SYSTEM_PROCESSOR "${ANAF_GNU_TARGET}")
endif()

if(NOT CMAKE_C_COMPILER)
    if(DEFINED ENV{ANAF_GCC} AND NOT "$ENV{ANAF_GCC}" STREQUAL "")
        set(CMAKE_C_COMPILER "$ENV{ANAF_GCC}")
    elseif(DEFINED ENV{CNOS_ANAF_GCC} AND NOT "$ENV{CNOS_ANAF_GCC}" STREQUAL "")
        set(CMAKE_C_COMPILER "$ENV{CNOS_ANAF_GCC}")
    else()
        set(_anaf_gcc_name "${ANAF_GNU_TARGET}-gcc")
        find_program(CMAKE_C_COMPILER NAMES "${_anaf_gcc_name}"
            HINTS
                "$ENV{ANAF_PREFIX}/bin"
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
