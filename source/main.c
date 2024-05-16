#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <wafel/dynamic.h>
#include <wafel/ios_dynamic.h>
#include <wafel/utils.h>
#include <wafel/patch.h>
#include <wafel/ios/svc.h>

#include "fs_patches.h"
#include "kernel_patches.h"
#include "mcp_patches.h"
#include "acp_patches.c"

// This fn runs before everything else in kernel mode.
// It should be used to do extremely early patches
// (ie to BSP and kernel, which launches before MCP)
// It jumps to the real IOS kernel entry on exit.
__attribute__((target("arm")))
void kern_main()
{
    debug_printf("Applying Mocha patches\n");
    apply_kernel_patches();
    apply_fs_patches();
    apply_mcp_patches();
    apply_acp_patches();
    debug_printf("Mocha patches finished\n");
}

// This fn runs before MCP's main thread, and can be used
// to perform late patches and spawn threads under MCP.
// It must return.
void mcp_main()
{

}
