CNOS 用户态骨架（完整）
========================

本目录构成「裸机 ELF → 内核装入 ring 3」的**最小可运行用户程序**约定，与 **Slime STD**、**CNAF**、**kernel** 分工如下。

目录一览
--------

  user/
    README.txt           本文件
    user.ld              ELF 链接脚本（入口 _start，装载地址 0x400000）
    BUILD-CNAF.txt       ANAF/CNAF CMake 用户子工程说明
    SYSCALL-ABI.txt      int $0x80 寄存器与调用号权威说明

    include/
      cnos_user.h        C freestanding：调用号、errno、内联 syscall

    cnos-rt/
      cnos_syscalls.asm  Slime extern "C" 汇编桩（与 slimec 产出 .o 链接）
      README.txt         运行时说明

    apps/
      README.txt           新建应用模板说明
      minimal.c            最小 C 示例（可复制改名）

    hello.c              C 演示（内核 CMake 亦可嵌入）
    cnafloader.sm        Slime「CNAFLOADER」文案（无参 **cnrun** 内嵌包 IMAGE；需 **CHASEROS_WITH_SLIME_USER**）
    hello.sm / hello-std.sm / slime-demo.sm / syscall-errno-demo.sm / cnos-first.sm / ipc-hl-demo.sm
                         Slime 演示（需 SLIME_PATH 指向 integrations/slime-for-chaseros/std）；**cnos-first.sm** 展示 runtime + io 基础设施形态

三条编译路径
------------

**1）仓库内核构建链（嵌入 hello.elf）**

  cmake --build build

  生成 **build/user/hello.elf**（来自 **hello.c**）。可选 **-DCHASEROS_WITH_SLIME_USER=ON** 嵌入 **hello_sm.elf**，且内嵌 **demo_cnaf.bin** 的 IMAGE 改为 **cnafloader.sm**（CNAFLOADER）。

**2）仅用户 ELF — CMake（user 子目录）**

  # 在仓库根；$PWD/cmake/... 避免工具链相对路径被解析到 build 目录
  cmake -S user -B build-cnaf-user -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/cnaf-anaf.cmake"
  cmake --build build-cnaf-user

  或使用 **bash scripts/build-cnaf-user.sh**（内部使用绝对路径）。

  产出 **build-cnaf-user/hello.elf**、**build-cnaf-user/minimal.elf**。

**3）Slime → ELF — 脚本**

  source scripts/cnos-slime-dev-env.sh
  bash scripts/cnos-slime-compile.sh user/hello-std.sm /tmp/out.elf

与 STD / 内核的契约
--------------------

  * C：**#include "cnos_user.h"**，编号与 **kernel/syscall_abi.h** 对齐。
  * Slime：**import "cnos/io"** 等，常量见 **integrations/slime-for-cnos/std/cnos/**。
  * 汇编桩：**cnos-rt/cnos_syscalls.asm** 随 Slime 程序链接；纯 C 可用 **cnos_user.h** 内联 **int $0x80**，无需 rt。
