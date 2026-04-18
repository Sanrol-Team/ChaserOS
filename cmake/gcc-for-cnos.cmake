# GCC For CNOS — CNAF 用户态 ELF（与 cnaf-anaf.cmake 等价，便于对外文档引用）
#
# Linux/macOS 宿主机上 CMake 配置用户 App 时使用：
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/gcc-for-cnos.cmake ...
#
# 安装版工具链：
#   source $PREFIX/bin/cnos-app-env.sh   # CMAKE_TOOLCHAIN_FILE 已指向安装目录内 cnaf-anaf.cmake

message(STATUS "GCC For CNOS: CNAF freestanding ELF (including cnaf-anaf profile)")
include("${CMAKE_CURRENT_LIST_DIR}/cnaf-anaf.cmake")
