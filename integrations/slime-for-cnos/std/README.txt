Slime CNOS STD（用户态标准库占位）
==================================

路径：**integrations/slime-for-cnos/std**（作为 **SLIME_PATH** 的一段）。

系统调用调用约定与编号权威说明：**user/SYSCALL-ABI.txt**；内核头文件：**kernel/syscall_abi.h**。  
完整用户态目录骨架（C / Slime / CMake）：**user/README.txt**。

使用方式
--------

1. 编译与链接时需带上运行时桩 **`user/cnos-rt/cnos_syscalls.asm`**（已由 **scripts/cnos-slime-compile.sh** / **cnos-slimec.sh** 自动汇编并链接）。

2. 设置模块搜索路径，例如在 CNOS 源码根：

     export SLIME_PATH="/绝对路径/CNOS/integrations/slime-for-cnos/std"

   （**SLIME_PATH** 支持用 **`:`** 或 **`;`** 分隔多段；每段 trim 后参与搜索。）

   若 **slimec** 不在 **PATH** 中，可指向 Slime 源码 release 产物，例如：

     export SLIMEC="/绝对路径/Slime-main/target/release/slimec"

   **scripts/cnos-slime-compile.sh** 会优先使用 **SLIMEC**。

3. 在应用程序开头：

     import "cnos/runtime"
     import "cnos/io"

   最小参考示例：**user/cnos-first.sm**（第一层基础设施演示）。

与用户态示例并行开发 CNOS Slime STD
------------------------------------

目标：一边写 **`user/*.sm`** 应用，一边在 **`std/cnos/*.sm`** 扩展 STD，互不阻塞。

环境（终端里固定一套，便于两条线切换）：

  在 CNOS 仓库根：

    source scripts/cnos-slime-dev-env.sh

  等价手动设置：

  export CNOS_ROOT="/绝对路径/CNOS"
  export SLIME_PATH="$CNOS_ROOT/integrations/slime-for-cnos/std"
  # slimec 在 PATH 中，或：PATH="$CNOS_ROOT/build/slime-tools:$PATH"

职责划分：

  * **应用线**：在 **`user/`** 放 **`hello-std.sm`** 或新 **`*.sm`**，通过 **`import "cnos/io"`** 等使用 STD；用 **`scripts/cnos-slime-compile.sh app.sm out.elf`** 产出 ELF（依赖 **GCC For CNOS** 提供的 **x86_64-elf-ld**）。
  * **STD 线**：只在 **`integrations/slime-for-cnos/std/cnos/`** 增加 **`*.sm`**（按模块路径 **`cnos/模块名`**）；需要新 syscall 时同步 **`user/cnos-rt/cnos_syscalls.asm`**、内核 **`kernel/isr.c`** 用户态编号，并在 **`cnos/constants.sm`** 暴露常量。
  * **契约**：**`extern "C"`** 符号名与汇编 **`global`** 一致；**SYS_*** 与内核约定一致。

推荐迭代：改 STD → 立刻用同一条 **`SLIME_PATH`** 编译 **`user/hello-std.sm`** 或你的应用做回归；无需提交即可验证。详细工具链说明见 **`toolchains/slime-for-cnos/README.txt`** 与 **`integrations/slime-for-cnos/ROADMAP.txt`**。

当前模块
--------

  * **cnos/io.sm** — `cnos_write` / `cnos_write_stdout` / `cnos_exit`，基于 **extern "C"** 调用 **`cnos_sys_*`**（定义在 **cnos_syscalls.asm**）。
  * **cnos/constants.sm** — **SYS_***、**CNOS_MAX_WRITE_BYTES**。
  * **cnos/errno.sm** — **EPERM** / **EBADF** / **EINVAL** / **EFAULT** / **ENOSYS**（正数 errno，与负 **RAX** 对应）。
  * **cnos/syscall.sm** — **cnos_syscall_errno** / **cnos_syscall_ok**（通用返回值解析）。
  * **cnos/io.sm** — **cnos_write** / **cnos_exit** 等。
  * **cnos/fd.sm** — **FD_STDIN** / **FD_STDOUT** / **FD_STDERR**。
  * **cnos/runtime.sm** — **O_RDONLY**、**cnos_init**、**cnos_abort**（第一层用户态基础设施）。
  * **cnos/ipc.sm** — 高层：**cnos_ipc_ping_call** / **cnos_ipc_call_hybrid** / **cnos_ipc_ping_hybrid_scratch** / **cnos_ipc_ping_pong_ok**；底层桩与 **cnos_msg_*** 缓冲辅助见 **user/cnos-rt/cnos_syscalls.asm**（阶段 4）。

说明：内置 **`print`** 已由 slimec **cnos** 目标直接生成 **`int 0x80`**；STD 用于 **缓冲区写入**（**cnos_write**）等与 **extern** 链接的场景。
