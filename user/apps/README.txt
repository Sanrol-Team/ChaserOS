新建 C 用户程序
================

1. 复制 **minimal.c** 为 **myapp.c**，修改 **\_start** 内逻辑。
2. 使用 **#include "cnos_user.h"**（CMake 已配置 **include/**）。
3. 在 **user/CMakeLists.txt** 中参照 **minimal_cnaf** 增加 **add_executable**，**OUTPUT_NAME** 设为 **myapp.elf**，**target_link_options** 与 hello 相同。

系统调用勿硬编码魔法数；编号与 **kernel/syscall_abi.h**、**user/SYSCALL-ABI.txt** 保持一致。
