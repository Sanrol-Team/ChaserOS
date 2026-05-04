#ifndef KERNEL_USER_H
#define KERNEL_USER_H

#include <stddef.h>
#include <stdint.h>
#include "isr.h"

/** ring 3 嵌入式任务 PID（跳入用户前由 user.c 设置） */
extern uint64_t chaseros_active_user_pid;

/** 与 Shell `cd` 同步的用户态工作目录（供 SYS_OPEN 相对路径解析） */
void chaseros_user_cwd_reset(void);
void chaseros_user_set_cwd(const char *abs_norm_path);
const char *chaseros_user_get_cwd(void);

void user_run_embedded_hello(void);

/** 嵌入的 Slime（hello.sm）用户 ELF；未启用 CHASEROS_HAVE_SLIME_USER 时为空实现 */
void user_run_embedded_slime_hello(void);

/** 解析内核内嵌的首个 CNPK（MANIFEST + CNIM IMAGE）并执行 */
void user_run_embedded_demo_cnaf(void);

/**
 * 解析暂存于物理连续页中的 .cnpk，装入 IMAGE 并跳入 ring 3。
 * 成功后不返回；失败时释放 blob 并打印诊断。
 */
void user_run_cnaf_from_owned_pages(uint8_t *blob, size_t blob_sz, uint64_t blob_pages,
                                    const char *tag);

/** 从 VFS 路径流式装载 .cnpk/.cnlk */
void user_run_cnaf_from_path_streaming(const char *abs_norm_path);

void chaseros_user_jump_ring3(uint64_t rip, uint64_t rsp);

void user_on_syscall_exit(struct registers *regs);

void userland_exit_done(void);

#endif
