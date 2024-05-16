#include <wafel/patch.h>

void apply_bsp_patches(void){

    // give us bsp::ee:read permission for PPC
    U32_PATCH_K(0xe6044db0, 0x000001F0);
 }