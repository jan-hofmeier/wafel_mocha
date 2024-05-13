#pragma once

#include "ipc_types.h"

int MCP_LoadFile_patch(ipcmessage *msg, int r1, int r2, int r3, int (*real_MCP_LoadFile)(ipcmessage *msg));