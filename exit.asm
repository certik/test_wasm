; exit.asm
.global _main
.align 4
_main:
        mov x1, #5
        mov x2, #6
        add x0, x1, x2
        ;mov x0, #42     ; exit code
        mov x16, #1     ; syscall number for exit
        svc #0x80       ; do the syscall!
