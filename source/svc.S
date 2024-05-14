.global svcCustomKernelCommand
.type svcCustomKernelCommand, %function
svcCustomKernelCommand:
	.word 0xE7F081F0
	bx lr