#include <wafel/patch.h>

#define FIND_USB_FLAG 0xE012202C

void apply_acp_patches(void){
    // allow custom bootLogoTex and bootMovie.h264
    ASM_PATCH_K(0xE0030D68, "mov r0, #0");
    ASM_PATCH_K(0xE0030D34, "mov r0, #0");

    // allow any region title launch
    ASM_PATCH_K(0xE0030498, "mov r0, #0");

    // Patch CheckTitleLaunch to ignore gamepad connected result
    ASM_PATCH_K(0xE0030868, "mov r0, #0");

    // force check USB storage on load
    U32_PATCH_K(FIND_USB_FLAG, 1);
}