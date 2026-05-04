#ifndef CHASEROS_CNDYN_LOADER_H
#define CHASEROS_CNDYN_LOADER_H

#include <stddef.h>
#include <stdint.h>

int cndyn_run_embedded_package(const uint8_t *blob, size_t blob_sz, const char *tag);
int cndyn_run_package_from_path(const char *abs_path);
int cndyn_load_package_for_exec(const char *abs_path, uint64_t *out_entry);
int cndyn_dlopen(uint64_t user_path_ptr, uint64_t mode);
int cndyn_dlsym(uint64_t handle, uint64_t user_symbol_ptr);
int cndyn_dlclose(uint64_t handle);

#endif
