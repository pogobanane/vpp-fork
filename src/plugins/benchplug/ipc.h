/*
 * TODO license smth
 */
#ifndef __included__ipc_h__
#define __included_ipc_h__

//#include <vnet/vnet.h>
//#include <vnet/ip/ip.h>
//#include <vnet/ethernet/ethernet.h>

#include <vppinfra/hash.h>
#include <vppinfra/error.h>
#include <vppinfra/elog.h>

#include <stdatomic.h>

#include <benchplug/ringbuf.h>

struct sample_ipc_mem_t;

/* 
 * message to client
 * client: read only
 * server: write
 */
typedef struct {
    u32 poll1;
    u32 udelay;
    u32 poll2;
    u32 usleep;
    u32 poll3;
    u32 use_interrupt; // may be used as boolean or as wait-for-interrupt-timeout
    u32 poll4;
} sample_ipc_for_client_t;

typedef struct {
    /* Filedescriptor describing the mmap */
    int fd;

    /* describes the mmapped memory */
    struct sample_ipc_mem_t *memory;

    /* read only message for client which is being
     * updated while communicating to server */
    sample_ipc_for_client_t last_response;

    int size;
} sample_ipc_main_t;

/* message sent from client to server 
 * messages from server to client are unint32_t
*/
typedef struct {
	uword n_rx_packets;
} sample_ipc_for_server_t;

extern int sample_ipc_open(sample_ipc_main_t *self);

clib_error_t* sample_ipc_close(sample_ipc_main_t *self);

void sample_ipc_communicate_to_server_prefetch(sample_ipc_main_t *self);

void sample_ipc_communicate_to_server(sample_ipc_main_t *self, uint16_t port_id, uint16_t queue_id, uint32_t n_rx_packets);

void sample_ipc_communicate_to_client(sample_ipc_main_t *self, sample_ipc_for_client_t *response, sample_ringbuffer_t* request);

#endif /* __included_ipc_h__ */
