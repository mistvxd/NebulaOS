BUILD = build
SRC = src
ISO = iso

CFLAGS = -m32 -ffreestanding -mpreferred-stack-boundary=2
LDFLAGS = -m elf_i386

PROGRAMS = editor welcome cd ls cat login clear gta6 whoami game shell

all: nebula.iso disk.img

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/boot.o: $(SRC)/boot.s | $(BUILD)
	nasm -f elf32 $< -o $@

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	gcc $(CFLAGS) -c $< -o $@

$(BUILD)/gdt_flush.o: $(SRC)/gdt_flush.asm | $(BUILD)
	nasm -f elf32 $< -o $@

$(BUILD)/idt_load.o: $(SRC)/idt_load.asm | $(BUILD)
	nasm -f elf32 $< -o $@

$(BUILD)/user_stub.o: $(SRC)/user_stub.asm | $(BUILD)
	nasm -f elf32 $< -o $@

KERNEL_OBJS = \
	$(BUILD)/boot.o \
	$(BUILD)/kernel.o \
	$(BUILD)/filesystem.o \
	$(BUILD)/disk.o \
	$(BUILD)/heap.o \
	$(BUILD)/paging.o \
	$(BUILD)/gdt_flush.o \
	$(BUILD)/gdt.o \
	$(BUILD)/idt_load.o \
	$(BUILD)/idt.o \
	$(BUILD)/tss.o \
	$(BUILD)/user_stub.o \
	$(BUILD)/syscall.o

$(BUILD)/kernel.bin: $(KERNEL_OBJS)
	ld $(LDFLAGS) -T $(SRC)/linker.ld -o $@ $^

$(BUILD)/%.o: $(SRC)/programs/%.asm | $(BUILD)
	nasm -f elf32 $< -o $@

$(BUILD)/%.elf: $(BUILD)/%.o
	ld $(LDFLAGS) -T $(SRC)/user_linker.ld -o $@ $^

$(SRC)/programs/%.bin: $(BUILD)/%.elf
	objcopy -O binary $< $@

programs: $(PROGRAMS:%=$(SRC)/programs/%.bin)

nebula.iso: $(BUILD)/kernel.bin
	mkdir -p $(ISO)/boot/grub
	cp $< $(ISO)/boot/kernel.bin
	echo 'set timeout=0' > $(ISO)/boot/grub/grub.cfg
	echo 'set default=0' >> $(ISO)/boot/grub/grub.cfg
	echo 'menuentry "nebula os" { multiboot /boot/kernel.bin }' >> $(ISO)/boot/grub/grub.cfg
	grub-mkrescue -o $@ $(ISO)

disk.img: programs
	dd if=/dev/zero of=$@ bs=512 count=100

	i=10; \
	for p in $(PROGRAMS); do \
		dd if=$(SRC)/programs/$$p.bin of=$@ bs=512 seek=$$i conv=notrunc; \
		i=$$((i+1)); \
	done

run: nebula.iso disk.img
	qemu-system-i386 -cdrom nebula.iso -d int,cpu_reset,guest_errors -hda disk.img -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD) $(ISO) *.iso disk.img