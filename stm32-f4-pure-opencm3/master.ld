
/* Generic linker script for STM32 targets using libopencm3. */

MEMORY {
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
    CCMRAM (xrw) : ORIGIN = 0x10000000, LENGTH = 64K
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Enforce emmition of the vector table. */
EXTERN (vector_table)

/* Define the entry point of the output file. */
ENTRY(reset_handler)

/* Define sections. */
SECTIONS
{
	.text : {
		*(.vectors)	/* Vector table */
		*(.text*)	/* Program code */
		. = ALIGN(4);
		*(.rodata*)	/* Read-only data */
		. = ALIGN(4);
	} >FLASH

	/* C++ Static constructors/destructors, also used for __attribute__
	 * ((constructor)) and the likes */

	.preinit_array : {
		. = ALIGN(4);
		__preinit_array_start = .;
		KEEP (*(.preinit_array))
		__preinit_array_end = .;
	} >FLASH

	.init_array : {
		. = ALIGN(4);
		__init_array_start = .;
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array))
		__init_array_end = .;
	} >FLASH

	.fini_array : {
		. = ALIGN(4);
		__fini_array_start = .;
		KEEP (*(.fini_array))
		KEEP (*(SORT(.fini_array.*)))
		__fini_array_end = .;
	} >FLASH

	/*
	 * Another section used by C++ stuff, appears when using newlib with
	 * 64bit (long long) printf support
	 */
	.ARM.extab : {
		*(.ARM.extab*)
	} >FLASH

	.ARM.exidx : {
		__exidx_start = .;
		*(.ARM.exidx*)
		__exidx_end = .;
	} >FLASH

	. = ALIGN(4);
	_etext = .;

	.data : {
		_data = .;
		*(.data*)	/* Read-write initialized data */
		. = ALIGN(4);
		_edata = .;
	} >SRAM AT >FLASH
	_data_loadaddr = LOADADDR(.data);

	.bss : {
		*(.bss*)	/* Read-write zero initialized data */
		*(COMMON)
		. = ALIGN(4);
		_ebss = .;
	} >SRAM

        /* CCM-RAM section 
         *
         * IMPORTANT NOTE! 
         * If variables placed in this section must be zero initialized,
         * the startup code needs to be modified to initialize this section.  
         */
        .ccmram : {
            . = ALIGN(4);
            _sccmram = .;       /* create a global symbol at ccmram start */
            *(.ccmram)
            *(.ccmram*)
            . = ALIGN(4);
            _eccmram = .;       /* create a global symbol at ccmram end */
        } >CCMRAM

	/*
	 * The .eh_frame section appears to be used for C++ exception handling.
	 * You may need to fix this if you're using C++.
	 */
	/* /DISCARD/ : { *(.eh_frame) } */

	. = ALIGN(4);
	end = .;
	__end = .;
}

PROVIDE(_estack = ORIGIN(SRAM) + LENGTH(SRAM));
PROVIDE(_stack = ORIGIN(SRAM) + LENGTH(SRAM));

/*
PROVIDE(_estack = ORIGIN(CCMRAM) + LENGTH(CCMRAM)); 
PROVIDE(_stack = ORIGIN(CCMRAM) + LENGTH(CCMRAM));
 */

PROVIDE(__heap = __end );
PROVIDE(__heap_start = __end);

