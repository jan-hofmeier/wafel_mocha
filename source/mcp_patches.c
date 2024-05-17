#include <wafel/patch.h>
#include <wafel/trampoline.h>
#include "mcp_loadfile.h"

void apply_mcp_patches(void){
    // fix 10 minute timeout that crashes MCP after 10 minutes of booting
    U32_PATCH_K(0x05022474, 0xFFFFFFFF);
    trampoline_t_blreplace(0x050254D6, MCP_LoadFile_patch);
    ASM_T_PATCH_K(0x05025242, "nop\nnop");
    trampoline_t_hook_before(0x05025242, MCP_ioctl100_patch);
    trampoline_t_blreplace(0x0501dd78, MCP_ReadCOSXml_patch);
    trampoline_t_blreplace(0x051105ce, MCP_ReadCOSXml_patch);

}