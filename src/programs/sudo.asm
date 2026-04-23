%define SYS_WRITE 2
%define SYS_READ 8
%define SYS_SPAWN_TASK 13
section .data
    msg1 db "antes de executar"
    msg1_len equ $ - msg1
    msg2 db "depois de executar"
    msg2_len equ $ - msg2

section .text
    global _start

_start:
    mov eax, SYS_WRITE
    mov ebx, msg1
    mov ecx, 0x0F
    mov edx, msg1_len
    int 0x80

    mov eax, SYS_SPAWN_TASK
    mov ebx, "welcome.bin"
    mov ecx, 0
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, msg2
    mov ecx, 0x0F
    mov edx, msg2_len
    int 0x80

    mov eax, 1
    int 0x80