section .data
nl db 10
nl_len equ 1

section .bss
buffer resb 16

section .text
global _start

_start:
    mov eax, 10
    mov ebx, buffer
    int 0x80

    mov esi, buffer
    call strlen

    mov edx, eax

    mov eax, 2
    mov ebx, buffer
    mov ecx, 0x0A
    int 0x80

    mov eax, 2
    mov ebx, nl
    mov ecx, 0x0F
    mov edx, nl_len
    int 0x80

    mov eax, 1
    int 0x80

strlen:
    xor eax, eax
.str_loop:
    cmp byte [esi + eax], 0
    je .end
    inc eax
    jmp .str_loop
.end:
    ret