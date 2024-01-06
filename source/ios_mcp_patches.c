/***************************************************************************
 * Copyright (C) 2016
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/

#include <wafel/patch.h>
#include "ios_mcp_patches.h"


void mcp_run_patches(void) {

    // Loadfile patch
    ASM_T_PATCH_K(0x050254D4, 
        "ldr r0, _lf_hook\n"
        "blx r0\n"
        "_lf_hook: .word MCP_LoadFile_patch\n"
    );

    // MCP_ioctl100_patch
    ASM_T_PATCH_K(0x05025242, 
        "ldr r2, _iocl100_hook\n"
        "blx r2\n"
        "_iocl100_hook: .word MCP_ioctl100_patch\n"
    );


    // ReadCOS patch
    ASM_T_PATCH_K(0x0501DD74, 
        "ldr r2, _cos_hook\n"
        "blx r2\n"
        "_cos_hook: .word MCP_ReadCOSXml_patch\n"
    );
    ASM_T_PATCH_K(0x051105CA, 
        "ldr r2, _cos_hook2\n"
        "blx r2\n"
        "_cos_hook2: .word MCP_ReadCOSXml_patch2\n"
    );


}
