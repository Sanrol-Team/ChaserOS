#ifndef CHASEROS_USER_RUNTIME_H
#define CHASEROS_USER_RUNTIME_H

#include <stddef.h>

long chaseros_write(int fd, const void *buf, size_t len);
long chaseros_read(int fd, void *buf, size_t len);
long chaseros_getpid(void);
void chaseros_exit(int code);

#endif
