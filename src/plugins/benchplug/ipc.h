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

#include <stdatomic.h>

struct sample_ipc_mem_t;

typedef struct {
    /* Filedescriptor describing the mmap */
    int fd;

    /* describes the mmapped memory */
    struct sample_ipc_mem_t *memory;

    int size;
} sample_ipc_main_t;

extern int sample_ipc_open(sample_ipc_main_t *self);

clib_error_t* sample_ipc_close(sample_ipc_main_t *self);

uint32_t sample_ipc_communicate_to_server(sample_ipc_main_t *self);

void sample_ipc_communicate_to_client(sample_ipc_main_t *self, uint32_t response);

#endif /* __included_ipc_h__ */
