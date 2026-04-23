section .data
nl db 10
nl_len equ 1

section .bss
buffer resb 512

section .text
global _start

_start:
    mov eax, [esp + 4]
    cmp eax, 2
    jl .exit

    mov ebx, [esp + 8]
    mov ebx, [ebx + 4]

    mov eax, 5
    mov ecx, 0
    int 0x80

    cmp eax, 0
    jl .exit

    mov ebx, eax

    mov eax, 6
    mov ecx, buffer
    mov edx, 512
    int 0x80

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

.exit:
    mov eax, 1
    int 0x80