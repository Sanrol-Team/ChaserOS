# CNOS

CNOS（CN OS）是基于 **64 位微内核** 的实验性操作系统，通过 **GRUB2 + Multiboot2** 从 ISO 启动；控制台为 **VGA 文本模式 + 串口**（Shell 经 PS/2 键盘 IRQ1）。存储侧正在接入 **e2fsprogs / libext2fs** 子集，内核提供 **VFS 根挂载**、裁剪 **ext2** 玩具卷与 **CNAF/CNAFL** 头校验。

## 特性（概览）

- **引导**：Multiboot2 头位于 `.text` 最前；可选用 `gfxpayload=text` 保持 VGA 文本显存可见。
- **控制台**：`0xB8000` 文本缓冲 + COM1 镜像输出。
- **内存**：物理页（PMM）、页表（VMM）；对用户缓冲区做 **可读/可写** 页表校验（系统调用路径）。
- **时钟**：IRQ0 递增单调 **`cnos_kernel_ticks`**，可供用户态 **`SYS_UPTIME_TICKS`** 查询。
- **VFS**：根卷需 **`mount`** 后，`ls` / `read` / `write` 才走 VFS；**`format`** 直接在当前块设备卷上创建极简 ext2（不要求已挂载）。详见下文「Shell 与卷」。
- **文件系统后端**：`kernel/fs` 内含裁剪后的 **e2fsprogs** 源码树（按需编入 CMake），说明见 `kernel/fs/README`。
- **IPC（实验）**：同步消息路径与 `process_find_by_pid`、发送队列、`msg_pending`（完整阻塞/调度仍在演进）。源码见 `kernel/ipc.c`。
- **用户态**：ring 3 嵌入 ELF（默认 C `user/hello.c`）；**`int $0x80`** 提供 **exit / write(1,2) / getpid / read(0,EOF) / uptime_ticks**。权威说明：**`kernel/syscall_abi.h`**、**`user/SYSCALL-ABI.txt`**。
- **Slime**：可选 **`CNOS_WITH_SLIME_USER`** 嵌入 **`slimec --target cnos`** 产物；标准库位于 **`integrations/slime-for-cnos/std`**，脚本见 **`scripts/cnos-slime-compile.sh`** 等。
- **应用包**：CNAF/CNAFL 规范与校验（`kernel/fs/cnaf/`）；用户 ELF 独立构建见 **`user/BUILD-CNAF.txt`**。

## Shell 与卷（常用流程）

块设备由 **`mkdisk`**（RAM）或 **`attach ide <0|1>`** 附加；成功后 **自动 `mount`**（也可用命令显式挂载）。**`vol`** 显示当前卷与 **VFS 是否已挂根**。

| 命令 | 作用 |
|------|------|
| `mount` / `umount` | 挂接或卸除 VFS 根（卸卷前 **`detach`** 会先 `umount`） |
| `format` | 在当前卷上创建单块组、1024 字节块的极简 ext2 |
| `ls` / `read <name>` / `write <name> <text>` | 需 VFS 已挂载；读写根目录下普通文件（由 ext2 后端实现） |

更全命令列表在 Shell 内执行 **`help`**。

## 依赖

| 工具 | 用途 |
|------|------|
| CMake ≥ 3.16 | 构建 |
| NASM | 内核汇编（`elf64`） |
| `x86_64-elf-gcc` + `x86_64-elf-ld` 等 | 裸机交叉编译（无 libc） |
| xorriso、`grub-mkrescue` | 生成 `cnos.iso`（可选；无则只生成 `kernel.elf`） |
| QEMU `qemu-system-x86_64` | 本地运行 ISO |

### 交叉工具链（多数 apt 源无预装包，需自行编译）

CNOS 需要 **目标三元组 `x86_64-elf`** 的裸机工具链（不是 `x86_64-linux-gnu`）。很多环境 **apt/dnf 里没有** 现成 `gcc-x86-64-elf`，需要 **从 GNU 源码编译 binutils + gcc**（或只用可信来源的预编译包）。

**与 CI 同源的一键脚本**（版本固定在脚本内：binutils / gcc 可从 GNU 镜像拉取）：

```bash
# 主机需已安装：gcc、g++、make、bison、flex、wget/curl、xz 等；脚本会在 gcc 源码树内运行 download_prerequisites 拉取 GMP/MPFR/MPC
sudo apt install build-essential bison flex texinfo curl xz-utils wget patch   # Debian/Ubuntu 示例

bash scripts/build-x86_64-elf-toolchain.sh "$HOME/opt/cross"
export PATH="$HOME/opt/cross/bin:$PATH"
```

若安装到仓库下的 `.cross`（与 GitHub Actions 默认一致）：

```bash
bash scripts/build-x86_64-elf-toolchain.sh "$(pwd)/.cross"
export PATH="$(pwd)/.cross/bin:$PATH"
```

更通用的手写步骤见 [OSDev Wiki: GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler)。

CMake 通过 `PATH` 查找 `x86_64-elf-gcc`，也可用 `CNOS_X86_64_ELF_GCC` 指向编译器可执行文件（见 `cmake/x86_64-elf.cmake`）。

**不要**用 `x86_64-linux-gnu-gcc` 代替；若个别发行版 **恰好** 提供 `gcc-x86-64-elf` 包，也可直接 `apt install` 后使用，与脚本二选一即可。

### 宿主上编译 CNOS 用户 ELF（可选）

在 Linux 上为 **用户态演示/工具链** 构建时，可参考 **`toolchains/linux-cnos-app/README.txt`** 与 **`scripts/build-cnos-app-linux-toolchain.sh`**（与内核用 **`x86_64-elf`** 裸机链区分用途）。

## 构建

在仓库根目录执行：

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-elf.cmake
cmake --build build
```

产物：

- `build/kernel.elf` — 内核 ELF（内嵌 `build/user/hello.elf`）  
- `build/cnos.iso` — 可启动镜像（需已安装 GRUB/xorriso）

若已配置过 `build/`且未换工具链，可直接：

```bash
cmake -B build
cmake --build build
```

交叉编译器不在默认路径时：

```bash
export CNOS_X86_64_ELF_GCC=/path/to/x86_64-elf-gcc
cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/x86_64-elf.cmake
cmake --build build
```

或使用 Makefile 封装：

```bash
make          # 首次自动带工具链文件配置
make clean    # 删除 build 目录
```

Slime 用户 ELF 嵌入内核：`-DCNOS_WITH_SLIME_USER=ON`（需 Slime 源码路径与 `slimec` / `nasm`，见根 `CMakeLists.txt`）。

等价预设：`cmake --preset default`（见 `CMakePresets.json`）。

## 运行（QEMU）

```bash
qemu-system-x86_64 -cdrom build/cnos.iso -m 128M -serial stdio
```

或：

```bash
make run
```

串口与 VGA 会同时输出；若只看终端，使用 `-serial stdio` 即可。

## 目录速览

| 路径 | 说明 |
|------|------|
| `kernel/` | 内核源码（C / NASM）；`ipc.c`、`fs/vfs.c`、`isr.c`（系统调用）、`user.c`（嵌入用户 ELF） |
| `kernel/fs/cnos/`、`kernel/fs/cnaf/` | ext2 卷适配、CNAF |
| `kernel/fs/lib/ext2fs/` 等 | e2fsprogs 库源码（按需加入 CMake） |
| `user/` | 用户态约定、**`user.ld`**、`hello.c`、CNAF 子工程、**`SYSCALL-ABI.txt`** |
| `integrations/slime-for-cnos/` | Slime for CNOS 标准库与路线图（`ROADMAP.txt`） |
| `cmake/x86_64-elf.cmake` | 裸机工具链 |
| `iso/boot/grub/grub.cfg` | GRUB 菜单（`multiboot2` 加载内核） |
| `scripts/` | 交叉链、Slime、CNAF 打包等脚本 |
| `.github/workflows/` | CI 与 Release（推送 `v*` tag 发布 ISO） |

更细的 **kernel/fs** 说明见 `kernel/fs/README`。

## 自动化（GitHub Actions）

- **不依赖** apt 里的 `gcc-x86-64-elf`：在 Runner 上执行 `scripts/build-x86_64-elf-toolchain.sh`，从 **GNU 官方源码** 编译 **binutils + gcc**（`x86_64-elf`），安装到 `${GITHUB_WORKSPACE}/.cross`。  
- **`actions/cache`** 缓存 `.cross` 目录；**首次运行**耗时会较长（完整编译工具链），**缓存命中**后只编译内核/ISO。  
- **CI**：推送/PR 至 `main` 时生成 `kernel.elf`、`cnos.iso`，并上传 Artifact。  
- **Release**：推送 `v*` 标签时同样先确保工具链，再发布 `cnos-<tag>.iso` 与 `kernel-<tag>.elf`。

自建 Runner 时：保证已安装脚本所需 **宿主编译依赖**（`build-essential`、`bison`、`flex`、`texinfo`、`curl`、`xz-utils` 等），与 Ubuntu 工作流一致即可。

## 语言与界面

- 控制台与 Shell 消息为 **英文** 为主；文档与注释可为 **中文**。  
- 图形界面（帧缓冲 GUI）当前默认不编入内核，便于稳定使用文本终端。

## 许可

上游 **e2fsprogs** 等文件遵循其原有 GPL/LGPL 等声明；仓库内 CNOS 新增文件以项目整体许可为准（若未单独声明，请与维护者确认）。

---

**后续方向（非承诺）**：VFS 多后端与路径解析、用户态 **open/read 文件**（接 VFS + fd 表）、IPC 与调度闭环、CNAF 按需加载等。
