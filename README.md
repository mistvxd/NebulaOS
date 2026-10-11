# NebulaOS

A tiny 32-bit x86 operating system written in C and NASM. It was my **first kernel**, built to learn how kernels work: booting, segmentation, interrupts, paging, user mode, syscalls, a disk driver and a filesystem.

It boots with GRUB, runs a small shell on the VGA text console, and launches user programs written in assembly from a persistent on-disk filesystem.

> **Status:** learning project, not under development. The code is kept as it was written, rough edges included. Its successors are [Sekura](https://github.com/mistvxd/Sekura) and later rewrites.

## Features

- **Boot:** Multiboot via GRUB, kernel loaded at 1 MiB
- **CPU setup:** GDT, TSS, IDT, PIC remapping
- **Paging:** identity-mapped first 16 MiB, separate user-accessible region for programs
- **User mode:** ring 3 execution via `iret`, `argc`/`argv` passed on the user stack
- **Syscalls:** `int 0x80` interface (see below)
- **Heap:** first-fit free-list allocator with block splitting and coalescing (64 KiB heap)
- **Disk:** ATA PIO driver (LBA28, sector read and write)
- **Filesystem:** in-memory file table with directories, owners and permission bits, saved to and loaded from disk
- **Users:** `root` plus per-user home directories under `/home/<user>`
- **Shell:** built into the kernel, with a `user@nebula:/path $` prompt, `PATH` lookup (`/bin`, then `/`) and automatic `.bin` suffix
- **Display and input:** VGA text mode 80x25 with colors and hardware cursor, polled PS/2 keyboard

## Repository layout

```
.
├── Makefile
├── disk.img              # prebuilt disk image with the user programs
├── grub/grub.cfg
└── src/
    ├── boot.s            # Multiboot entry
    ├── kernel.c          # init, VGA, keyboard, shell, user-mode entry
    ├── gdt.c, gdt_flush.asm
    ├── idt.c, idt_load.asm
    ├── tss.c
    ├── paging.c
    ├── heap.c
    ├── disk.c            # ATA PIO driver
    ├── filesystem.c
    ├── syscall.c
    ├── user_stub.asm
    ├── linker.ld         # kernel linker script
    ├── user_linker.ld    # user program linker script
    ├── headers/
    └── programs/         # user programs (.asm sources and built .bin)
```

## Building

### Requirements

- `gcc` with 32-bit support (`-m32`)
- `nasm`
- `ld` and `objcopy` (binutils)
- `grub-mkrescue` (with `xorriso` and the GRUB PC modules)
- `qemu-system-i386`

### Commands

```sh
make            # build nebula.iso and disk.img
make programs   # rebuild only the user programs (.bin)
make run        # build and run in QEMU
make clean      # remove build output, ISO and disk image
```

`make run` starts QEMU with the ISO as CD-ROM and `disk.img` as the hard disk, and logs interrupts and CPU resets.

## How it works

1. GRUB loads the kernel and jumps to `_start`, which calls `kernel_main`.
2. The kernel sets up GDT, TSS, IDT, PIC and paging, then the heap.
3. User programs are read from raw disk sectors 10-19 of `disk.img` and installed into `/bin` of the filesystem. Each program is stored in a single 512-byte sector, so **every program must fit in 512 bytes**.
4. The filesystem is saved back to disk and the shell starts.
5. When a command is entered, the shell finds the program, copies it to `0x200000`, and jumps to it in ring 3. The program ends with the `exit` syscall, which returns control to the shell.

Programs run one at a time. There is no multitasking.

## Syscalls

Invoked with `int 0x80`, number in `eax`, arguments in `ebx`, `ecx`, `edx`, `esi`.

| #  | Name               | Arguments                                | Description                         |
|----|--------------------|------------------------------------------|-------------------------------------|
| 1  | `SYS_EXIT`         | none                                     | End the program                     |
| 2  | `SYS_WRITE`        | `ebx` ptr, `ecx` color, `edx` length     | Print to the screen                 |
| 3  | `SYS_FS_READDIR`   | `ebx` path, `ecx` index, `edx` dirent    | Read the n-th entry of a directory  |
| 4  | `SYS_FS_CHDIR`     | `ebx` path                               | Change directory                    |
| 5  | `SYS_FS_OPEN`      | `ebx` path, `ecx` mode                   | Open a file, returns its index      |
| 6  | `SYS_FS_READ`      | `ebx` file index, `ecx` buffer, `edx` max| Read file contents                  |
| 7  | `SYS_FS_WRITE`     | `ebx` file index, `ecx` buffer, `edx` size | Replace file contents and save    |
| 8  | `SYS_READ`         | `ebx` buffer                             | Read one character from the keyboard|
| 9  | `SYS_LOGIN`        | `ebx` username                           | Switch user                         |
| 10 | `SYS_GET_CURRENT_USER` | `ebx` buffer                         | Get the current user name           |
| 11 | `SYS_TERMINAL_CLEAR` | none                                   | Clear the screen                    |
| 12 | `SYS_MOVE_CURSOR`  | `ebx` x, `ecx` y                         | Move the cursor                     |

## User programs

Written in NASM as flat binaries (`src/programs/*.asm`):

| Program   | What it does                                                           |
|-----------|------------------------------------------------------------------------|
| `welcome` | Prints the welcome message at boot                                     |
| `ls`      | Lists a directory                                                      |
| `cd`      | Changes directory                                                      |
| `cat`     | Prints a file                                                          |
| `editor`  | Writes one line (up to 63 characters) into a file, replacing its content |
| `clear`   | Clears the screen                                                      |
| `whoami`  | Prints the current user                                                |
| `login`   | Switches user: `login <name>`                                          |
| `gta6`    | Creates 100 small files (`file1` to `file100`), a filesystem stress test |
| `game`    | Placeholder that exits immediately                                     |

`reboot` is the only command built into the shell. `sudo.asm` is an unfinished experiment and is not built.

## Known limitations

This is a first kernel, so it has plenty:

- No authentication: `login` accepts any name with no password, and permission bits are not enforced against the current user
- No memory isolation: parts of the low kernel region are mapped user-accessible, so user programs can reach kernel memory
- Syscall pointer checks only validate the address range, not whether the pages are mapped
- Single program at a time, no scheduler, no `fork` or `exec`
- Polled keyboard, US layout only
- File names are limited to 15 characters, and programs to 512 bytes
