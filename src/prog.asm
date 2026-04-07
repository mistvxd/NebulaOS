[bits 32]

section .data
    msg db "hello world", 0
    color db 10

section .text
    global _start

_start:
    mov eax, 2
    mov ebx, msg
    mov ecx, color
    int 0x80

    mov eax, 1
    int 0x80