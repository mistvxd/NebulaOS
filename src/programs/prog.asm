[bits 32]
section .data
msg db "digite algo guloso: "
msg_len equ $ - msg
msg2 db "salvo em test.txt"
msg2_len equ $ - msg2
filename db "/test.txt", 0
nl db 10
nl_len equ 1

section .bss
buffer resb 64

section .text
global _start

_start:
    mov eax, 2
    mov ebx, msg
    mov ecx, 0x0F
    mov edx, msg_len
    int 0x80

    mov eax, 8
    mov ebx, buffer
    mov ecx, 64
    int 0x80

    mov esi, eax

    mov edx, eax
    mov eax, 2
    mov ebx, buffer
    mov ecx, 0x0F
    int 0x80

    mov eax, 2
    mov ebx, nl
    mov ecx, 0x0F
    mov edx, nl_len
    int 0x80

    mov eax, 5
    mov ebx, filename
    mov ecx, 1
    int 0x80

    mov ebx, eax
    mov eax, 7
    mov ecx, buffer
    mov edx, esi
    int 0x80

    mov eax, 1
    int 0x80