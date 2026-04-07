[bits 32]

section .data
    msg1 db "rosas sao vermelhas"
    len1 equ $ - msg1
    msg2 db "violetas sao azuis"
    len2 equ $ - msg2
    msg3 db "seu sistema operacional ira explodir"
    len3 equ $ - msg3

section .text
    global _start

_start:
    mov eax, 2
    mov ebx, msg1
    mov ecx, 0x0C
    mov edx, len1
    int 0x80

    mov eax, 2
    mov ebx, msg2
    mov ecx, 0x09
    mov edx, len2
    int 0x80

    mov eax, 2
    mov ebx, msg3
    mov ecx, 0x0D
    mov edx, len3
    int 0x80

    mov eax, 1
    int 0x80