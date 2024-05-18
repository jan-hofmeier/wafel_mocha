.arm

.extern fsa_ioctl0x28_hook

.global asm_fsa_ioctl0x28_hook
asm_fsa_ioctl0x28_hook:
    mov r0, r10
    mov r1, r11
    bl fsa_ioctl0x28_hook
    mov r5,#0x0
    ldr pc, =0x10701194
