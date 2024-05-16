#pragma once

#include <wafel/ios/ipc_types.h>
#include "ipc_defs.h"

int MCP_LoadFile_patch(ipcmessage *msg, int r1, int r2, int r3, int (*real_MCP_LoadFile)(ipcmessage *msg));

void MCP_ioctl100_patch(trampoline_t_state *state);

int MCP_ReadCOSXml_patch(uint32_t u1, uint32_t u2, MCPPPrepareTitleInfo *xmlData, int r3, int (*real_MCP_ReadCOSXml_patch)(uint32_t, uint32_t, MCPPPrepareTitleInfo*));