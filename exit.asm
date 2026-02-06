; exit.asm
.global _main
.align 4
_main:
        mov x0, #1                      ; fd = stdout
        adrp x1, msg@PAGE               ; buf = &msg
        add x1, x1, msg@PAGEOFF
        mov x2, #msg_len                ; count = msg_len
        bl _write

        mov x0, #42                     ; exit code
        bl _exit

.section __TEXT,__cstring,cstring_literals
msg:
        .asciz "hello from libSystem write()\n"
.set msg_len, . - msg - 1
