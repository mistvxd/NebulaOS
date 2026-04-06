all:
	mkdir -p build

	nasm -f elf32 src/boot.s -o build/boot.o
	gcc -m32 -ffreestanding -c src/kernel.c -o build/kernel.o
	gcc -m32 -ffreestanding -c src/filesystem.c -o build/filesystem.o
	gcc -m32 -ffreestanding -c src/disk.c -o build/disk.o
	gcc -m32 -ffreestanding -c src/heap.c -o build/heap.o
	nasm -f bin src/prog.asm -o src/prog.bin
	nasm -f elf32 src/gdt_flush.asm -o build/gdt_flush.o
	gcc -m32 -ffreestanding -c src/gdt.c -o build/gdt.o
	nasm -f elf32 src/idt_load.asm -o build/idt_load.o
	gcc -m32 -ffreestanding -c src/idt.c -o build/idt.o
	gcc -m32 -ffreestanding -c src/tss.c -o build/tss.o
	nasm -f elf32 src/user_stub.asm -o build/user_stub.o
	gcc -m32 -ffreestanding -c src/syscall.c -o build/syscall.o

	ld -m elf_i386 -T src/linker.ld -o build/kernel.bin build/boot.o build/kernel.o build/filesystem.o build/disk.o build/heap.o build/gdt_flush.o build/gdt.o build/idt_load.o build/idt.o build/tss.o build/user_stub.o build/syscall.o

	mkdir -p iso/boot/grub
	cp build/kernel.bin iso/boot/kernel.bin

	echo 'set timeout=0' > iso/boot/grub/grub.cfg
	echo 'set default=0' >> iso/boot/grub/grub.cfg
	echo 'menuentry "kiwi os" { multiboot /boot/kernel.bin }' >> iso/boot/grub/grub.cfg

	grub-mkrescue -o kiwi.iso iso

run:
	qemu-system-i386 -cdrom kiwi.iso -hda disk.img -no-reboot -display gtk,gl=off

clean:
	rm -rf build iso *.iso

disk:
	dd if=/dev/zero of=disk.img bs=512 count=100
	dd if=src/prog.bin of=disk.img bs=512 seek=10 conv=notrunc