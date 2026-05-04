CNOS 交叉编译链 — 文档索引
==========================

面向在 **Linux 宿主机** 上开发 **CNOS 内核（x86_64-elf）** 与 **用户态 CNAF/Slim e（无 libc ELF）**。

## 一键安装（推荐）

  bash scripts/build-cnos-cross-toolchain.sh "$HOME/opt/cnos-sdk"
  bash scripts/build-cnos-cross-toolchain.sh "$HOME/opt/cnos-sdk" --with-slime

然后：

  source "$HOME/opt/cnos-sdk/bin/cnos-cross-env.sh"

等价于装好 **GCC For CNOS + CNAF SDK + CMake 片段 + user.ld**，可选同一前缀下的 **slimec**。

## 分项说明

| 组件 | 脚本 / 文档 |
|------|----------------|
| GCC（ANAF）、CNAF 头、gcc-for-cnos.cmake | **scripts/build-cnos-app-linux-toolchain.sh**，**toolchains/gcc-for-cnos/README.txt**，**toolchains/linux-cnos-app/README.txt** |
| 仅安装 slimec 到自定义前缀 | **scripts/build-cnos-slime-toolchain.sh** |
| Slime → ELF / .cnaf | **scripts/cnos-slime-compile.sh**，**scripts/cnos-slimec.sh**，**toolchains/slime-for-cnos/README.txt** |
| 内核 CMake（Multiboot） | **cmake/x86_64-elf.cmake**，**CMakePresets.json** |
| ChaserOS 原生 GCC 引导链（x86_64-chaseros） | **scripts/build-chaseros-native-gcc.sh**，**toolchains/gcc-for-chaseros/README.txt**，**cmake/x86_64-chaseros-gcc.cmake** |

## 环境变量（cnos-cross-env.sh）

  CNOS_CROSS_ROOT / CNOS_APP_ROOT — 工具链前缀  
  CNOS_ROOT — 本仓库根目录（打包时写入）  
  CMAKE_TOOLCHAIN_FILE — gcc-for-cnos.cmake  
  CNOS_APP_SDK_INCLUDE — CNAF 规范头  
  CNOS_SLIMEC — slimec（若已安装）  
  CNOS_USER_LD — 默认 user.ld  

## Slime 源码

  bash scripts/fetch-slime-upstream.sh  

默认目录：**integrations/slime-for-cnos/upstream/slime**（须含 **`--target cnos`** 的 slimec）。
