
#include <wafel/trampoline.h>
#include <wafel/patch.h>
#include <wafel/ios/svc.h>
#include "fsa.h"

#define PATCHED_CLIENT_HANDLES_MAX_COUNT 0x40

// Disable raw access to every device except ODD and USB
#define DISABLED_CAPABILITIES                                                                                                       \
    (FSA_CAPABILITY_SLCCMPT_RAW_OPEN | FSA_CAPABILITY_SLC_RAW_OPEN | FSA_CAPABILITY_MLC_RAW_OPEN | FSA_CAPABILITY_SDCARD_RAW_OPEN | \
     FSA_CAPABILITY_HFIO_RAW_OPEN | FSA_CAPABILITY_RAMDISK_RAW_OPEN | FSA_CAPABILITY_OTHER_RAW_OPEN)


int (*const get_handle_from_val)(uint32_t) = (void *) 0x107046d4;


typedef struct __attribute__((packed)) {
    ipcmessage ipcmessage;
} ResourceRequest;

FSAClientHandle *patchedClientHandles[PATCHED_CLIENT_HANDLES_MAX_COUNT];

static int fsa_ioctlv_hook(ResourceRequest *param_1, uint32_t u2, uint32_t u3, int r3,  int (*org_ioctlv)(ResourceRequest *, uint32_t, uint32_t)){
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(param_1->ipcmessage.fd);
    uint64_t oldValue             = clientHandle->processData->capabilityMask;
    int toBeRestored              = 0;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            clientHandle->processData->capabilityMask = 0xffffffffffffffffL & ~DISABLED_CAPABILITIES;
            // printf("IOCTL: Force mask to 0xFFFFFFFFFFFFFFFF for client %08X\n", (uint32_t) clientHandle);
            toBeRestored = 1;
            break;
        }
    }
    int res = org_ioctlv(param_1, u2, u3);

    if (toBeRestored) {
        // printf("IOCTL: Restore mask for client %08X\n", (uint32_t) clientHandle);
        clientHandle->processData->capabilityMask = oldValue;
    }

    return res;
}

static int fsa_ioctl_hook(ResourceRequest *request, uint32_t u2, uint32_t u3, uint32_t u4, int (*org_ioctl)(ResourceRequest *, uint32_t, uint32_t, uint32_t)) {
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(request->ipcmessage.fd);
    uint64_t oldValue             = clientHandle->processData->capabilityMask;
    int toBeRestored              = 0;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            // printf("IOCTL: Force mask to 0xFFFFFFFFFFFFFFFF for client %08X\n", (uint32_t) clientHandle);
            clientHandle->processData->capabilityMask = 0xffffffffffffffffL & ~DISABLED_CAPABILITIES;
            toBeRestored                              = 1;
            break;
        }
    }
    int res = org_ioctl(request, u2, u3, u4);

    if (toBeRestored) {
        // printf("IOCTL: Restore mask for client %08X\n", (uint32_t) clientHandle);
        clientHandle->processData->capabilityMask = oldValue;
    }

    return res;
}

static int fsa_ios_close_hook(uint32_t fd, ResourceRequest *request, int r2, int r3, int (*org_ios_close)(uint32_t, ResourceRequest*)) {
    FSAClientHandle *clientHandle = (FSAClientHandle *) get_handle_from_val(fd);
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == clientHandle) {
            // printf("Close: %p will be closed, reset slot %d\n", clientHandle, i);
            patchedClientHandles[i] = 0;
            break;
        }
    }
    return org_ios_close(fd, request);
}

void fsa_ioctl0x28_hook(trampoline_state *s) {
    FSAClientHandle *handle = (FSAClientHandle*)s->r[10];
    void *request = (void*)s->r[11];
    int res = -5;
    for (int i = 0; i < PATCHED_CLIENT_HANDLES_MAX_COUNT; i++) {
        if (patchedClientHandles[i] == handle) {
            res = 0;
            break;
        }
        if (patchedClientHandles[i] == 0) {
            patchedClientHandles[i] = handle;
            res                     = 0;
            break;
        }
    }

    iosResourceReply(request, res);
    //s->r[5] = 0;
    s->r[0] = 0;
    s->lr = 0x10701194;
}

void apply_fs_patches(void){
    U32_PATCH_K(0x10701248, "mov r5, #0");
    trampoline_hook_before(0x10701248, fsa_ioctl0x28_hook);
    trampoline_blreplace(0x10704540, fsa_ioctlv_hook);
    trampoline_blreplace(0x107044f0, fsa_ioctl_hook);
    trampoline_blreplace(0x10704458, fsa_ios_close_hook);

    // patch /dev/odm IOCTL 0x06 to return the disc key if in_buf[0] > 2.
    ASM_PATCH_K(0x10739948, 
                "mov r11, #1\n"
                "mov r7, #0x20\n"
    );
    BRANCH_PATCH_K(0x10739950, 0x107399a8);

    //TODO FAT32 patches

    
}