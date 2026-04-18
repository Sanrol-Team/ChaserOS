# CNAF 用户态 ELF — 独立工程，须使用 cmake/cnaf-anaf.cmake（ANAF + CNAF 标志）
cmake_minimum_required(VERSION 3.16)

project(cnos_cnaf_user C)

# 已安装 CNOS App 工具链时由 cnos-app-env.sh 设置；否则使用源码树 kernel/fs/cnaf
if(DEFINED ENV{CNOS_APP_SDK_INCLUDE} AND NOT "$ENV{CNOS_APP_SDK_INCLUDE}" STREQUAL "")
  set(_cnaf_spec_default "$ENV{CNOS_APP_SDK_INCLUDE}")
else()
  set(_cnaf_spec_default "${CMAKE_CURRENT_SOURCE_DIR}/../kernel/fs/cnaf")
endif()
set(CNAF_SPEC_DIR "${_cnaf_spec_default}" CACHE PATH "CNAF 规范头（cnaf_spec.h）")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")

# 示例应用：*.elf → 可作为 .cnaf 包内 IMAGE 节载荷；共用 user.ld
# 用法：cnos_user_elf_target(<CMake目标名> <源文件> <输出文件名不含后缀>)
macro(cnos_user_elf_target _tgt _src _out)
  add_executable(${_tgt} ${_src})
  set_target_properties(${_tgt} PROPERTIES OUTPUT_NAME ${_out}.elf)
  target_include_directories(${_tgt} PRIVATE "${CNAF_SPEC_DIR}")
  target_link_options(${_tgt} PRIVATE
      -nostdlib
      "-T${CMAKE_CURRENT_SOURCE_DIR}/user.ld"
  )
endmacro()

cnos_user_elf_target(hello_cnaf hello.c hello)
cnos_user_elf_target(minimal_cnaf apps/minimal.c minimal)

# 可按需：cnos_user_elf_target(myapp_cnaf apps/myapp.c myapp)
