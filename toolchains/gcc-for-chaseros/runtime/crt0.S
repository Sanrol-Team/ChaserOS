.global _start
.type _start, @function

/* Minimal ChaserOS user entry:
 *   _start -> main() -> sys_exit(return_code)
 */
_start:
    xorq %rbp, %rbp
    call main
    movl %eax, %edi
    movl $0, %eax          /* CHASEROS_SYS_EXIT */
    int $0x80
1:
    hlt
    jmp 1b
