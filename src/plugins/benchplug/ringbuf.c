/*
 * TODO license smth
 */
#include <fcntl.h>
#include <benchplug/ringbuf.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <vppinfra/cache.h>

// TODO use rbuf->sizeOfBuffer after creation during runtime

void sample_ringbuf_init(sample_ringbuffer_t* rbuf)
{
	memset(rbuf, 0, sizeof(sample_ringbuffer_t));
	rbuf->sizeOfBuffer = SAMPLE_RINGBUF_SIZE;
	sample_ringbuf_reset(rbuf);
}

void sample_ringbuf_reset(sample_ringbuffer_t *rbuf)
{
	rbuf->headIndex = 0;
	rbuf->tailIndex = 0;
}

void sample_ringbuf_push_prefetch(sample_ringbuffer_t *rbuf)
{
	CLIB_PREFETCH(rbuf, CLIB_CACHE_LINE_BYTES, LOAD);
	CLIB_PREFETCH(rbuf, CLIB_CACHE_LINE_BYTES, STORE);
}

// push onto top
void sample_ringbuf_push(sample_ringbuffer_t *rbuf, uint32_t data)
{
	rbuf->n_rx_packets[rbuf->headIndex] = data;

	// update headIndex
	// this acts as (idx+1)%size because size is a power of two
	rbuf->headIndex += 1;
	rbuf->headIndex &= SAMPLE_RINGBUF_MAP;
	if (rbuf->headIndex == rbuf->tailIndex )
	{
		// if we overwrite a 
		rbuf->tailIndex += 1;
		rbuf->tailIndex &= SAMPLE_RINGBUF_MAP;
	}
}

// remove from bottom
// return number of returned uwords (0 or 1)
uint8_t sample_ringbuf_pop(sample_ringbuffer_t *rbuf, uint32_t *dst)
{
	if (rbuf->headIndex == rbuf->tailIndex) // rbuf is empty
		return 0;

	uint32_t n_rx_packets = rbuf->n_rx_packets[rbuf->tailIndex];
	rbuf->tailIndex += 1;
	rbuf->tailIndex &= SAMPLE_RINGBUF_MAP;

	*dst = n_rx_packets;
	return 1;
}

// requirement: sizeof(*n_rx_packets) == rbuf->sizeOfBuffer
// uword *n_rx_packets is the destination
// returns number of uwords copied
uint32_t sample_ringbuf_extract_all(sample_ringbuffer_t *rbuf, uint32_t *n_rx_packets)
{
	// TODO not implemented
	return 1;

	//uint32_t start_idx = rbuf->tailIndex;
	if (rbuf->tailIndex <= rbuf->headIndex)
	{
		uint32_t n_copy = rbuf->headIndex - rbuf->tailIndex;
		memcpy(n_rx_packets, rbuf->n_rx_packets , sizeof(uint32_t) * n_copy);
	} else // tailIndex > headIndex
	{
		uint32_t n_copy = rbuf->headIndex - rbuf->tailIndex;
		memcpy(n_rx_packets, rbuf->n_rx_packets, sizeof(uint32_t) * n_copy);
		//memcpy()
	}
	//uint32_t stop_idx = min(start_idx, )
}