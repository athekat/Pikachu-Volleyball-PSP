	.section .rodata, "a", @progbits
	.align 4
	.global bgm_raw_start
bgm_raw_start:
	.incbin "assets/bgm.raw"
