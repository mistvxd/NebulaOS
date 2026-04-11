[BITS 32]

%define SYS_FS_CHDIR 4

section .text
global _start

_start:
    mov eax, [esp + 4]
    cmp eax, 2
    jl .exit

    mov ebx, [esp + 8]
    mov ebx, [ebx + 4]

    mov eax, SYS_FS_CHDIR
    int 0x80

.exit:
    mov eax, 1
    int 0x80