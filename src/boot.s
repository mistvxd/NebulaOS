section .multiboot
align 4
    dd 0x1BADB002
    dd 0x00
    dd -(0x1BADB002 + 0x00)

section .text
global _start
extern kernel_main

_start:
    cli
    push 1
    call kernel_main

.hang:
    hlt
    jmp .hang