/*
 * TODO license smth
 */
#ifndef __included__ipc_h__
#define __included_ipc_h__

#include <vnet/vnet.h>
#include <vnet/ip/ip.h>
#include <vnet/ethernet/ethernet.h>

#include <vppinfra/hash.h>
#include <vppinfra/error.h>
#include <vppinfra/elog.h>

typedef struct {
    /* Filedescriptor describing the mmap */
    int fd;

    /* Memory pointer to mmap */
    void *memory;

    int size;
} sample_ipc_main_t;

int sample_ipc_open(sample_ipc_main_t *self);

clib_error_t* sample_ipc_close(sample_ipc_main_t *self);

#endif /* __included_ipc_h__ */
