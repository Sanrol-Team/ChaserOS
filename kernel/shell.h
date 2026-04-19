/* kernel/shell.h - 基础 Shell 功能 */

#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#define MAX_COMMAND_LEN 512

/* 初始化 Shell */
void shell_init();

/* 处理键盘输入并更新缓冲区 */
void shell_handle_input(char c);

/* 执行命令 */
void shell_execute(const char *cmd);

/** 当前 Shell 工作目录（绝对路径，与内核用户态 cwd 同步） */
const char *shell_get_cwd(void);

#endif
