#include "vmm.h"

#include <stdint.h>

/*
 * COW phase-1:
 * Kernel records intent but current address-space model is shared 2MiB mappings.
 * Keep explicit hook so page fault path and fork ABI are wired now; physical split
 * can be completed without touching syscall/loader/scheduler interfaces again.
 */
void vmm_mark_process_cow(uint64_t pml4_phys) {
    (void)pml4_phys;
}

int vmm_try_handle_cow_fault(uint64_t fault_addr, uint64_t err_code) {
    (void)fault_addr;
    (void)err_code;
    return -1;
}
