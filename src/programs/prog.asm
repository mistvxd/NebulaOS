[BITS 32]

%define SYS_EXIT        1
%define SYS_WRITE       2
%define SYS_FS_READDIR  3
%define SYS_FS_CHDIR    4
%define SYS_FS_OPEN     5
%define SYS_FS_READ     6
%define SYS_FS_WRITE    7

section .data
filename db "/test.txt", 0
msg db "hello", 0
nl db 10

section .bss
buf resb 64

section .text
global _start

_start:
    mov eax, SYS_FS_OPEN
    mov ebx, filename
    mov ecx, 1
    int 0x80

    mov esi, eax

    mov eax, SYS_FS_WRITE
    mov ebx, esi
    mov ecx, msg
    mov edx, 5
    int 0x80

    mov eax, SYS_FS_OPEN
    mov ebx, filename
    mov ecx, 0
    int 0x80

    mov esi, eax

    mov eax, SYS_FS_READ
    mov ebx, esi
    mov ecx, buf
    mov edx, 64
    int 0x80

    mov edx, eax

    mov eax, SYS_WRITE
    mov ebx, buf
    mov ecx, 0x0F
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, nl
    mov ecx, 0x0F
    mov edx, 1
    int 0x80

    mov eax, SYS_EXIT
    int 0x80