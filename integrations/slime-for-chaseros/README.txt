Slime For CNOS（CNOS 应用层主要语言）
======================================

**定位**：后续 CNOS 用户态（CNAF App）开发与生态的**主要编程语言为 Slime**。
内核、引导与底层仍可按需使用 C/汇编；面向应用开发者，以 Slime 为第一选择。

上游编译器：**slimec**（Rust），默认将 Slime 源码编译为 **x86-64 NASM**，再经 `nasm` + 链接器生成宿主 OS 可执行文件。这与 CNOS 所需的 **裸机 ELF64（x86_64-elf，无 libc）** 不同，必须在 Slime 侧增加 **CNOS 目标** 或 **汇编后处理 + GCC For CNOS 链接** 流水线。

参考仓库（示例）：[FORGE24/Slime](https://github.com/FORGE24/Slime)。本地树布局见 **SLIME-SRC-OVERVIEW.txt**。

## Slime `src/` 概览（编译器本体）

* **main.rs**：入口、`slimec` 管线（词法/语法/模块/所有权/优化/CodeGen）、**Target**（当前含 linux-x64 / windows-x64 / macos-x64）、输出 NASM。
* **ctfe.rs / ctfe_extended.rs**：编译期求值相关。
* **tce.rs / ifm.rs / dope.rs / dynamic_precomp.rs / scheduler_elimination.rs / pre_concurrency.rs**：各类分析与优化 Pass。

细节与模块职责见 **SLIME-SRC-OVERVIEW.txt**。

## 当前阶段（过渡）

1. **C + GCC For CNOS**：仍可用于内核携带样例、`user/hello.elf`、与加载器验证（见 `user/`、`toolchains/gcc-for-cnos/`）。
2. **Slime**：在 **slimec** 增加 **CNOS** 目标（或固定 NASM → `x86_64-elf-ld` + `user.ld`）后，经 **CNAB** 封装作为 **CNAF IMAGE** 主路径（内核不解析 ELF）。

## CNOS 对接要点

* **链接契约**：`user/user.ld`，入口 `_start`，**GCC For CNOS**（`cmake/gcc-for-cnos.cmake`）。
* **包格式**：CNAF（`kernel/fs/cnaf/cnaf_spec.h`）。
* **建议实现顺序**：见 **ROADMAP.txt**（已按「slimec → NASM」现状修订）。

## 工具链（slimec + GCC For CNOS）

  * **一键交叉链**：**scripts/build-cnos-cross-toolchain.sh**（见 **toolchains/README.txt**）
  * 说明：**toolchains/slime-for-cnos/README.txt**；集成说明以本目录 **integrations/slime-for-chaseros** 为准。
  * 构建 slimec 并安装：`bash scripts/build-cnos-slime-toolchain.sh <前缀>`
  * 编译 `.sm` → 用户 ELF：`bash scripts/cnos-slime-compile.sh user/hello.sm out.elf`
  * slimec 须支持 **`--target cnos`**（见上游 Slime `Target::CnosX64`；若官方尚未合并，使用含 CNOS 补丁的 slimec）。
  * **内核**：可选 **`cmake -DCHASEROS_WITH_SLIME_USER=ON`** 将 **`user/hello.sm`** 编入内核镜像，shell 命令 **`slime`** 运行；并 **`user/cnafloader.sm`** → **`cnafloader.cnab`**（由 ELF 经 objcopy+wrap-cnab 生成）作为 **内嵌 `demo_cnaf.bin` 的 IMAGE**（无参 **`cnrun`** 即运行 Slime CNAFLOADER，替代默认 C **`hello.cnab`** 包）。默认在同一构建内 **`cargo build`** 编译上游 Slime 得到 **`build/slime-tools/slimec`**（**CHASEROS_BUILD_SLIMEC**）；见 **upstream/README.txt**。
  * **宿主打包 CNAF（ChaserOS 仓库内）**：**`bash scripts/slime-pack-cnaf.sh ./app.sm ./app.cnaf`**（**cnos-slime-compile.sh** → **objcopy** → **wrap-cnab.py** → **pack-cnaf.py**）；依赖 **slimec --target cnos**、**nasm**、**x86_64-elf-ld**、**x86_64-elf-objcopy**、**Python3**。说明见 **user/BUILD-CNAF.txt**。
  * **旧脚本名**（若仍存在）：**`scripts/cnos-slimec.sh`** 等；**内核 shell 无法执行 slimec**，仅在开发机终端打包。
  * **宿主打包 CNAFL（静态库单元）**：**`bash scripts/cnos-slimec-lib.sh ./lib.sm ./out.cnafl --lib-name mylib`**；或用 **`scripts/slimec ./lib.sm ./out.cnafl --lib-name mylib`**。脚本将 `.sm` → `.o` → **`ar`** → **MANIFEST + IMAGE(.a)**。供应用 CNAF `requires=` 占位；内核当前不加载 `.cnafl`。
  * **库语义（非可执行 ELF）**：slimec 使用 **`--target cnos --emit static-lib`** 生成**无 `_start`** 的 NASM，再 `nasm -felf64` + `ar`；默认可执行程序仍为 **`--emit exe`**（与 **`cnos-slime-compile.sh`** 一致）。

## 运行 CNAF（内核已实现）

构建时会把 **`pack-cnaf.py`** 生成的首个演示包 **`demo_cnaf.bin`**（IMAGE 为 **CNAB**：默认 C **`hello.cnab`**；若 **`CHASEROS_WITH_SLIME_USER=ON`** 则为 Slime **`cnafloader.cnab`**）链进 **`kernel.elf`**。启动后在 Shell 输入 **`cnrun`**（无参数）即可解析 CNAF、装入 CNAB 并在 ring 3 跳转入口。
若已有挂载的 ext2，可将宿主生成的 **`*.cnaf`** 拷入卷根，执行 **`cnrun <文件名>`**。

## 拉取上游开发

  bash scripts/fetch-slime-upstream.sh

克隆到 **integrations/slime-for-cnos/upstream/slime**（若已存在则跳过）。
