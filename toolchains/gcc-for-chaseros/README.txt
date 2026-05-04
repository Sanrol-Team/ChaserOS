GCC For ChaserOS（原生 GCC 开发引导链）
=======================================

本目录对应 ChaserOS 的“原生 GCC”开发路线起点。

当前阶段（bootstrap）提供：

  * 目标三元组：x86_64-chaseros
  * Binutils + GCC（freestanding，without headers）
  * sysroot 骨架目录 + 最小 headers（`usr/include/chaseros/`）
  * 最小启动与运行时：`crt0.o`、`libchaseros_user.a`
  * CMake toolchain 文件：cmake/x86_64-chaseros-gcc.cmake

一键构建：

  bash scripts/build-chaseros-native-gcc.sh "$HOME/opt/chaseros-native-gcc"
  source "$HOME/opt/chaseros-native-gcc/bin/chaseros-native-gcc-env.sh"
  x86_64-chaseros-gcc --version

最小程序链接自检：

  bash scripts/test-chaseros-native-link.sh "$HOME/opt/chaseros-native-gcc"

设计目标（后续）：

  1) 完善 ChaserOS libc（当前仅最小运行时）
  2) 支持 hosted 模式与标准头
  3) 逐步推进到“在 ChaserOS 上原生运行 GCC”

注意：

  * 现阶段是“原生编译器研发链”，不是完整用户态发行工具链。
  * 与现有 x86_64-elf（ANAF）链并行存在，互不替代。
