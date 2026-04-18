GCC For CNOS（面向 CNAF / CNOS App 的交叉 GCC 发行名）
========================================================

「GCC For CNOS」是本仓库对同一套裸机交叉工具链的产品化称呼。

**语言策略**：CNOS 用户态应用以 **Slime** 为主要开发语言（编译器 **slimec**，见 Slime `src/`）；
在 Slime 目标 **CNOS** 成熟前，用户样例与引导仍可使用 **C + 本工具链**。详见 **integrations/slime-for-cnos/**。

**技术关系**简述：

  * 编译器本体：GNU Binutils + GCC，目标三元组 **x86_64-elf**（无宿主 libc），
    构建脚本即 **scripts/build-anaf-cross-toolchain.sh**（内部亦称 ANAF）。
  * CNAF 用户程序：使用 **cmake/cnaf-anaf.cmake** 或 **cmake/gcc-for-cnos.cmake**，
    产出可装入 `.cnaf`「IMAGE」节的 ELF64。
  * Linux 上一键安装（GCC + CNAF 头文件 + CMake + 环境脚本）：
    **scripts/build-cnos-app-linux-toolchain.sh**

安装后环境变量（见 **bin/cnos-app-env.sh**）：

  CNOS_APP_ROOT / CNOS_APP_TOOLCHAIN_ROOT / GCC_FOR_CNOS_ROOT（均指向安装前缀）
  CMAKE_TOOLCHAIN_FILE → share/cnos/cmake/gcc-for-cnos.cmake（源码树同路径亦可）
  CNOS_APP_SDK_INCLUDE → cnos-sysroot/include

与「linux-cnos-app」文档目录的关系：**linux-cnos-app** 描述 Linux 宿主安装流程；
**gcc-for-cnos** 描述对外名称与组件对应关系。
