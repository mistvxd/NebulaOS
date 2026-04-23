[BITS 32]

%define SYS_EXIT  1
%define SYS_LOGIN 9

section .text
global _start

_start:
    mov eax, [esp + 4]
    cmp eax, 2
    jl .exit

    mov ebx, [esp + 8]
    mov ebx, [ebx + 4]

    mov eax, SYS_LOGIN
    int 0x80

    jmp .exit

.exit:
    mov eax, SYS_EXIT
    int 0x80