#include <wafel/ios/svc.h>
#include "smd.h"

//#include <imports.h>
#include <string.h>

static SmdIopContext contexts[8];

static void *smdPhysToVirt(SmdIopContext *ctx, uint32_t paddr) {
    return (void *) (paddr + ctx->memVirtOffset);
}

static uint32_t smdVirtToPhys(SmdIopContext *ctx, void *vaddr) {
    return (uint32_t) vaddr + ctx->memPhysOffset;
}

int32_t smdIopInit(int32_t index, SmdControlTable *table, const char *name, int lock) {
    if (index > 7) {
        return -0xc0003;
    }

    SmdIopContext *ctx = &contexts[index];

    // Verify table pointer
    if (iosCheckIosAddrRange(table, sizeof(SmdControlTable), MEM_PERM_RW) != 0) {
        return -0xc0007;
    }

    ctx->controlTable = table;

    if (lock && !ctx->hasSemaphore) {
        ctx->semaphore = iosCreateSemaphore(1, 1);
        if (ctx->semaphore < 0) {
            return -0xc0008;
        }

        ctx->hasSemaphore = 1;
    }
    ctx->shouldLock = lock;

    iosInvalidateDCache(table, sizeof(SmdControlTable));

    if (strncmp(table->name, name, sizeof(table->name)) != 0) {
        return -0xc0007;
    }

    // Calculate virt and phys offsets
    uint32_t paddr     = iosVirtToPhys(table);
    ctx->memVirtOffset = (int32_t) table - paddr;
    ctx->memPhysOffset = paddr - (int32_t) table;

    ctx->iopElementBuf   = (SmdElement *) smdPhysToVirt(ctx, table->iopInterface.bufPaddr);
    ctx->iopElementCount = table->iopInterface.elementCount;

    ctx->ppcElementBuf   = (SmdElement *) smdPhysToVirt(ctx, table->ppcInterface.bufPaddr);
    ctx->ppcElementCount = table->ppcInterface.elementCount;

    // Verify element bufffers
    if (iosCheckIosAddrRange(ctx->iopElementBuf, ctx->iopElementCount * sizeof(SmdElement), MEM_PERM_RW) != 0 ||
        iosCheckIosAddrRange(ctx->ppcElementBuf, ctx->ppcElementCount * sizeof(SmdElement), MEM_PERM_RW) != 0) {
        return -0xc0007;
    }

    ctx->state = SMD_STATE_INITIALIZED;
    return 0;
}

int32_t smdIopOpen(int32_t index) {
    if (index > 7) {
        return -0xc0003;
    }

    SmdIopContext *ctx = &contexts[index];
    if (ctx->state < SMD_STATE_INITIALIZED) {
        return -0xc000d;
    }

    // Update interface state
    ctx->controlTable->iopInterface.state = SMD_INTERFACE_STATE_OPENED;
    iosFlushDCache(&ctx->controlTable->iopInterface.state, 4);

    ctx->state = SMD_STATE_OPENED;
    return 0;
}

int32_t smdIopClose(int32_t index) {
    if (index > 7) {
        return -0xc0003;
    }

    SmdIopContext *ctx = &contexts[index];
    if (ctx->state < SMD_STATE_INITIALIZED) {
        return -0xc000d;
    }

    // Update interface state
    ctx->controlTable->iopInterface.state = SMD_INTERFACE_STATE_CLOSED;
    iosFlushDCache(&ctx->controlTable->iopInterface.state, 4);

    ctx->state = SMD_STATE_CLOSED;
    return 0;
}

int32_t smdIopGetInterfaceState(int32_t index, SmdInterfaceState *iopState, SmdInterfaceState *ppcState) {
    if (index > 7) {
        return -0xc0003;
    }

    SmdIopContext *ctx = &contexts[index];
    if (ctx->state < SMD_STATE_INITIALIZED) {
        return -0xc000d;
    }

    // Read interface states
    if (ppcState) {
        iosInvalidateDCache(&ctx->controlTable->ppcInterface.state, 4);
        *ppcState = (SmdInterfaceState) ctx->controlTable->ppcInterface.state;
    }

    if (iopState) {
        iosInvalidateDCache(&ctx->controlTable->iopInterface.state, 4);
        *iopState = (SmdInterfaceState) ctx->controlTable->iopInterface.state;
    }

    return 0;
}

int32_t smdIopReceive(int32_t index, SmdReceiveData *data) {
    int32_t ret = 0;
    int32_t incoming;

    if (index > 7) {
        return -0xc0003;
    }

    SmdIopContext *ctx  = &contexts[index];
    SmdInterface *iface = &ctx->controlTable->iopInterface;

    memset(data, 0, sizeof(SmdReceiveData));

    if (ctx->shouldLock) {
        iosWaitSemaphore(ctx->semaphore, 0);
    }

    iosInvalidateDCache(iface, sizeof(SmdInterface));

    if (iface->readOffset > (int32_t) ctx->iopElementCount) {
        ret = -0xc0007;
        goto done;
    }

    // figure out how many incoming elements we can receive
    if (iface->readOffset < iface->writeOffset) {
        incoming = iface->writeOffset - iface->readOffset;
    } else if (iface->writeOffset < iface->readOffset) {
        incoming = iface->elementCount - iface->readOffset;
        if (iface->writeOffset > 0) {
            incoming += iface->writeOffset;
        }
    } else {
        ret = -0xc0005;
        goto done;
    }

    if (incoming <= 0) {
        ret = -0xc0005;
        goto done;
    }

    SmdElement *element = &ctx->iopElementBuf[iface->readOffset];
    iosInvalidateDCache(element, sizeof(SmdElement));

    data->size = element->size;
    data->type = element->type;

    // handle message types
    if (element->type == SMD_ELEMENT_TYPE_MESSAGE) {
        if (element->size > 0x80) {
            ret = -0xc0001;
            goto done;
        }

        memcpy(data->message, element->data, element->size);
    } else if (element->type == SMD_ELEMENT_TYPE_VECTOR_SPEC) {
        SmdVector *vec   = &element->spec;
        data->spec.count = vec->count;
        data->spec.id    = vec->id;

        if (data->spec.count > 4) {
            ret = -0xc0002;
            goto done;
        }

        // Translate and verify spec pointers
        for (int i = 0; i < data->spec.count; ++i) {
            SmdVectorSpec *spec = &data->spec.vecs[i];
            spec->ptr           = smdPhysToVirt(ctx, (uint32_t) vec->vecs[i].ptr);
            spec->size          = vec->vecs[i].size;

            if (iosCheckIosAddrRange(spec->ptr, spec->size, MEM_PERM_RW) != 0) {
                ret = -0xc000f;
                goto done;
            }

            iosInvalidateDCache(spec->ptr, spec->size);
        }
    } else if (element->type == SMD_ELEMENT_TYPE_VECTOR) {
        SmdVector *vec = (SmdVector *) smdPhysToVirt(ctx, element->vectorPaddr);
        iosInvalidateDCache(vec, sizeof(SmdVector));

        if (vec->count > 4) {
            ret = -0xc0002;
            goto done;
        }

        data->vector = vec;

        // Translate and verify pointers
        for (int i = 0; i < vec->count; ++i) {
            SmdVectorSpec *spec = &data->vector->vecs[i];
            spec->ptr           = smdPhysToVirt(ctx, (uint32_t) spec->ptr);

            if (iosCheckIosAddrRange(spec->ptr, spec->size, MEM_PERM_RW) != 0) {
                ret = -0xc000f;
                goto done;
            }

            iosInvalidateDCache(spec->ptr, spec->size);
        }
    } else {
        ret = -0xc0006;
        goto done;
    }

    // Increment read offset
    iface->readOffset = (iface->readOffset + 1) % iface->elementCount;
    iosFlushDCache(&iface->readOffset, 4);

done:;
    if (ctx->shouldLock) {
        iosSignalSemaphore(ctx->semaphore);
    }

    return ret;
}

static int32_t writeElement(SmdIopContext *ctx, SmdElementType type, void *data, uint32_t size) {
    int32_t ret = 0;
    int32_t outgoing;

    if (ctx->shouldLock) {
        iosWaitSemaphore(ctx->semaphore, 0);
    }

    SmdInterface *iface = &ctx->controlTable->ppcInterface;
    iosInvalidateDCache(iface, sizeof(SmdInterface));

    if (iface->writeOffset > (int32_t) ctx->ppcElementCount) {
        ret = -0xc0007;
        goto done;
    }

    // figure out how many outgoing messages we can send
    if (iface->writeOffset < iface->readOffset) {
        outgoing = iface->readOffset - iface->writeOffset;
    } else {
        outgoing = iface->elementCount - iface->writeOffset;
        if (iface->readOffset >= 0) {
            outgoing += iface->readOffset;
        }
    }

    if (outgoing <= 0) {
        goto done;
    }

    // write data to element
    SmdElement *element = &ctx->ppcElementBuf[iface->writeOffset];
    element->type       = type;
    element->size       = size;
    memcpy(element->data, data, size);

    iosFlushDCache(element, sizeof(SmdElement));

    // Increment write offset
    iface->writeOffset = (iface->writeOffset + 1) % iface->elementCount;
    iosFlushDCache(&iface->writeOffset, 4);

done:;
    if (ctx->shouldLock) {
        iosSignalSemaphore(ctx->semaphore);
    }

    return ret;
}

int32_t smdIopSendVector(int32_t index, SmdVector *vector) {
    if (index > 7) {
        return -0xc0003;
    }

    SmdIopContext *ctx = &contexts[index];

    if (vector->count > 4) {
        return -0xc0002;
    }

    // flush and translate pointers
    for (int i = 0; i < vector->count; ++i) {
        SmdVectorSpec *spec = &vector->vecs[i];
        iosFlushDCache(spec->ptr, spec->size);

        spec->ptr = (void *) smdVirtToPhys(ctx, spec->ptr);
    }

    iosFlushDCache(vector, sizeof(SmdVector));

    // write vector paddr as element
    uint32_t paddr = smdVirtToPhys(ctx, vector);
    return writeElement(ctx, SMD_ELEMENT_TYPE_VECTOR, &paddr, sizeof(paddr));
}
