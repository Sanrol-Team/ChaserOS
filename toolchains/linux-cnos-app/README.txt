Linux 宿主机 → CNOS App（CNAF 用户 ELF）GCC 工具链
=================================================

对外名称：**GCC For CNOS**（说明见 toolchains/gcc-for-cnos/README.txt）。

目标：在任意常见 x86_64 Linux 上安装一套固定前缀的交叉编译器（x86_64-elf-gcc），专用于
编译可装入 CNAF「IMAGE」节的裸机 ELF64（无宿主 glibc），与内核用户 ABI 一致。

一键安装 **完整交叉链**（GCC + 可选 slimec，推荐）：

  bash scripts/build-cnos-cross-toolchain.sh "$HOME/opt/cnos-sdk"
  bash scripts/build-cnos-cross-toolchain.sh "$HOME/opt/cnos-sdk" --with-slime
  source "$HOME/opt/cnos-sdk/bin/cnos-cross-env.sh"

仅 **GCC For CNOS / CNAF SDK**（与旧流程相同，耗时较长）：

  bash scripts/build-cnos-app-linux-toolchain.sh "$HOME/opt/cnos-app"

安装内容：

  bin/x86_64-elf-gcc, ld, …     （ANAF）
  bin/cnos-app-env.sh           （source 后设置 PATH / CMAKE / SDK）
  cnos-sysroot/include/         （cnaf_spec.h、cnaf.h）
  share/cnos/cmake/{anaf,cnaf-anaf}.cmake
  share/cnos/ld/user.ld         （默认链接脚本副本，可自行复制修改）
  share/cnos/VERSION

日常使用：

  source "$HOME/opt/cnos-app/bin/cnos-app-env.sh"
  bash scripts/build-cnaf-user.sh

或在自有工程中指：

  cmake -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" ...

说明：本bundle 与纯 ANAF 的差别在于附带 CNAF SDK 头、CMake 安装副本及统一环境脚本；
GCC 本体仍来自 scripts/build-anaf-cross-toolchain.sh。
