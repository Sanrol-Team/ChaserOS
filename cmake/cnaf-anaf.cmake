# CNAF + ANAF — 用于生成可装入 .cnaf「IMAGE」节的裸机 ELF64（无 libc）
#
# 在 cnaf_spec.h 中：主载荷推荐 ELF64（CNOS 子集 ABI），目标三元组与内核一致。
# 本文件先加载 ANAF（交叉 GCC），再追加 CNAF 用户态默认 C/LD 标志。
#
# 典型用法（先安装 ANAF 或设置 PATH）:
#   cmake -S user -B build-cnaf-user -DCMAKE_TOOLCHAIN_FILE=cmake/cnaf-anaf.cmake
#   cmake --build build-cnaf-user
#
# 或与内核同一前缀:
#   source "$PREFIX/anaf-toolchain.env"
#   cmake -S user -B build-cnaf-user -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/cnaf-anaf.cmake"

include("${CMAKE_CURRENT_LIST_DIR}/anaf.cmake")

# 编译：与内核用户示例一致（freestanding，无宿主 libc）
string(APPEND CMAKE_C_FLAGS_INIT " -ffreestanding -O2 -Wall -Wextra -m64 -fno-builtin -fno-stack-protector ")

# 链接：裸机可执行，由 user.ld 布置段；无 crt0
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -nostdlib ")

set(CNOS_CNAF_TOOLCHAIN "cnaf-anaf" CACHE STRING "CNOS CNAF user toolchain profile" FORCE)
