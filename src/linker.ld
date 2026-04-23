ENTRY(_start)

SECTIONS {
    . = 1M;

    .text : {
        *(.multiboot)
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss :
    {
        *(.bss)
    
        heap_start = .;
        . = . + 0x10000;
        heap_end = .;
    }
}