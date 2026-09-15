	.section .rodata, "a", @progbits
	.align 4
	.global sfx_bin_start
sfx_bin_start:
	.incbin "assets/sfx.bin"
