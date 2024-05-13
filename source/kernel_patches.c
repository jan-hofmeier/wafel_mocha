#include <wafel/patch.h>
#include "thread.h"
#include "kernel_commands.h"
#include "kernel_utils.h"

ThreadContext_t **currentThreadContext = (ThreadContext_t **) 0x08173ba0;
uint32_t *domainAccessPermissions      = (uint32_t *) 0x081a4000;

static int kernel_syscall_0x81(u32 command, u32 arg1, u32 arg2, u32 arg3) {
    void (*invalidate_icache)()                           = (void (*)()) 0x0812DCF0;
    void (*invalidate_dcache)(unsigned int, unsigned int) = (void (*)()) 0x08120164;
    void (*flush_dcache)(unsigned int, unsigned int)      = (void (*)()) 0x08120160;
    int result                                            = 0;
    int level                                             = disable_interrupts();
    set_domain_register(domainAccessPermissions[0]); // 0 = KERNEL

    switch (command) {
        case KERNEL_READ32: {
            result = *(volatile u32 *) arg1;
            break;
        }
        case KERNEL_WRITE32: {
            *(volatile u32 *) arg1 = arg2;
            flush_dcache(arg1, 4);
            invalidate_icache();
            break;
        }
        case KERNEL_MEMCPY: {
            kernel_memcpy((void *) arg1, (void *) arg2, arg3);
            flush_dcache(arg1, arg3);
            invalidate_icache();
            break;
        }
        case KERNEL_READ_OTP: {
            int (*read_otp_internal)(int index, void *out_buf, u32 size) = (int (*)(int, void *, u32)) 0x08120248;
            read_otp_internal(0, (void *) (arg1), 0x400);
            break;
        }
        default: {
            result = -1;
            break;
        }
    }

    set_domain_register(domainAccessPermissions[(*currentThreadContext)->pid]);
    enable_interrupts(level);

    return result;
}


void apply_kernel_patches(void){
    // is this needed?
    // patch kernel dev node registration
    U32_PATCH_K(0x081430B4, 1);

    BRANCH_PATCH_K(0x0812CD2C, kernel_syscall_0x81);
}