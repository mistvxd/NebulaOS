[BITS 32]

section .data
    msg db "Welcome to NebulaOS!", 10
    len equ $ - msg

section .text
    global _start

_start:
    mov eax, 2
    mov ebx, msg
    mov ecx, 0x09
    mov edx, len
    int 0x80

    mov eax, 1
    int 0x80