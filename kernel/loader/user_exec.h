#ifndef CHASEROS_USER_EXEC_H
#define CHASEROS_USER_EXEC_H

#include <stdint.h>
#include "isr.h"

long user_sys_spawn(uint64_t user_path_ptr);
long user_sys_exec(uint64_t user_path_ptr, struct registers *regs);
long user_sys_waitpid(long target_pid, uint64_t user_status_ptr);
long user_sys__exit(long code, struct registers *regs);
long user_sys_fork(struct registers *regs);

#endif
