Slime for CNOS — 原生应用层语言工具链
====================================

**定位**：CNOS 在应用层提供以 **Slime** 为首选的、**随仓库/随安装包分发的** 语言与编译器（`slimec`）组合。与仅作「宿主编 C 再链进裸机」的交叉 C 不同，**slimec 是 CNOS 生态内第一条「从源码到 CNAF 用户 ELF」的高层编译路径**。

**组件**

1. **slimec**（Rust）：Slime → x86-64 NASM。须支持 **`--target cnos`**（或 `cnos-x64`），生成与 **GCC For CNOS** 链接兼容的 ELF64 指令（用户态 **`int $0x80`**，与 `user/hello.c` 一致）。
2. **nasm**：`nasm -felf64` 得到 `.o`。
3. **GCC For CNOS**（ANAF）：`x86_64-elf-ld -nostdlib -T user/user.ld` 链接为 **hello.elf**。
4. **可选**：`.cnaf` 打包仍可用既有 CNAF 脚本/流程。

**Slime CNOS STD（`integrations/slime-for-cnos/std`）**

  * 模块 **`cnos/io`**：`cnos_write` / `cnos_write_stdout` / `cnos_exit`（基于 **extern "C"** + **`user/cnos-rt/cnos_syscalls.asm`**）。
  * **`export SLIME_PATH=$CNOS/integrations/slime-for-cnos/std`**（也可用 `:` 分隔多路径）。
  * 编译脚本 **`cnos-slime-compile.sh`** / **`cnos-slimec.sh`** 默认链接 **`cnos_syscalls.asm`**（可用 **`CNOS_SLIME_LINK_RT=`** 空 关闭）。
  * 示例：**`user/hello-std.sm`**。
  * 需在 Slime **≥ 本次 CNOS 协作的 import 修复**：`import` 会合并被导入模块中的 **`extern`**（见 Slime `process_imports`）。

**在 CNOS 工程内编译 Slime（推荐）**

  bash scripts/fetch-slime-upstream.sh
  cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-elf.cmake -DCNOS_WITH_SLIME_USER=ON
  cmake --build build

默认 **CNOS_BUILD_SLIMEC=ON**：在 **CNOS_SLIME_SRC**（默认 `integrations/slime-for-cnos/upstream/slime`）执行 **`cargo build --release`**，生成 **`build/slime-tools/slimec`**，再编译 `user/hello.sm`，**不要求 PATH 里有 slimec**。仍需 **Rust/cargo**、**nasm**、以及内核同一套 **x86_64-elf-ld**。

若改用已安装的 slimec：**`-DCNOS_BUILD_SLIMEC=OFF`**，必要时 **`-DCNOS_SLIMEC=/path/to/slimec`**。

Slime 源码必须支持 **`--target cnos`**；说明见 **integrations/slime-for-cnos/upstream/README.txt**。

构建后在 shell 执行 **`slime`** 运行嵌入的 `user/hello.sm`；未启用 **`CNOS_WITH_SLIME_USER`** 时 **`slime`** 会提示未嵌入。

**一键构建 slimec + 环境脚本**

  bash scripts/build-cnos-slime-toolchain.sh "$HOME/opt/cnos-slime"

前提：已安装 **Rust/cargo**，且 Slime 源码可用（见下）。

**一行：`.sm` → `.cnaf`（宿主 Linux 终端；内核里的 CNOS shell 不能跑 slimec）**

  bash scripts/cnos-slimec.sh ./test.sm ./test.cnaf

**一行：`.sm` → `.cnafl`（IMAGE 为 `ar` 静态归档；需 `--lib-name`）**

  bash scripts/cnos-slimec-lib.sh ./stub.sm ./stub.cnafl --lib-name stub

或使用仓库 **`scripts/slimec`**：`./test.sm ./test.cnaf` 自动走 CNAF；`./lib.sm ./lib.cnafl --lib-name foo` 走 CNAFL。

若希望命令形如 **`slimec ./test.sm ./test.cnaf`**：把仓库 **`scripts/`** 加到 `PATH` 后使用其中的 **`slimec` 包装脚本**（仅当「恰好两个参数且输出以 `.cnaf` 结尾」时走 CNAF；其它参数仍交给真正的 slimec）。

**一键：从 .sm 到 ELF（不打包 CNAF）**

  source "$HOME/opt/cnos-slime/bin/cnos-slime-env.sh"
  bash scripts/cnos-slime-compile.sh path/to/app.sm path/to/out.elf

或指定 Slime 源码根：

  SLIME_SRC=/path/to/Slime bash scripts/build-cnos-slime-toolchain.sh ...

**Slime 源码位置（任选其一）**

  * **integrations/slime-for-cnos/upstream/slime** — `bash scripts/fetch-slime-upstream.sh` 拉取上游；
  * 环境变量 **SLIME_SRC** 指向本地克隆（须含 **cnos** 目标的 slimec；上游若尚未合并，请使用含 CNOS 补丁的分支）。

**与 `toolchains/linux-cnos-app` 的关系**

  * **linux-cnos-app / GCC For CNOS**：裸机 ELF 链接与 CNAF 头 — **两者共用**。
  * **slime-for-cnos**：在之上增加 **slimec**，形成「Slime 源码 → ELF」的完整前端。

详见 **integrations/slime-for-cnos/README.txt** 与 **ROADMAP.txt**。
