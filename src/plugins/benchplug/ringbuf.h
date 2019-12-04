/*
 * TODO license smth
 */
#ifndef __included__ringbuf_h__
#define __included_ringbuf_h__

// inspired by https://stackoverflow.com/a/1771607/4108673

#include <stdint.h>

/*
12Mpps / 256 max. badge size =~ 47k badges per second
=> 47k uwords/second
=> 47k uwords/sec * 4 byte = 188kB/sec
47k uwords/sec * 10 sec = 470k uwords
188kB/sec * 10 sec = 1.88MB

assuming a memcpy speed of 15000MB/s, a ringbuf memcpy takes:
https://stackoverflow.com/questions/21038965/why-does-the-speed-of-memcpy-drop-dramatically-every-4kb
1048576*4bytes = 4MB
4MB / 15000MB/s = 0.267ms
10Mpps * 0.267ms = 2.67K packets (0.0267% of all 
	packets can't be processed immediately because auf the memcpy)
=> no significant loss of throughput in high load scenarios
=> probably enough packet buffer in low load scenarios
*/
// has to be a power of two and smaller than 2^32
#define SAMPLE_RINGBUF_SIZE 1048576
#define SAMPLE_RINGBUF_MAP 1048575 // SAMPLE_RINGBUF_SIZE - 1
// capacity will be SAMPLE_RINGBUF_SIZE - 1

typedef struct {
	uint16_t port_id;
	uint16_t queue_id;
	uint32_t n_rx_packets;
} sample_ringbuffer_data_t;

typedef struct {
	uint32_t headIndex; // next free field
	uint32_t tailIndex; // oldest field or =headIndex if ring is empty
	uint32_t sizeOfBuffer;
	sample_ringbuffer_data_t data[SAMPLE_RINGBUF_SIZE];
} sample_ringbuffer_t;

void sample_ringbuf_init(sample_ringbuffer_t* rbuf);

void sample_ringbuf_reset(sample_ringbuffer_t *rbuf);

void sample_ringbuf_push_prefetch(sample_ringbuffer_t *rbuf);

void sample_ringbuf_push(sample_ringbuffer_t *rbuf, uint16_t port_id, uint16_t queue_id, uint32_t n_rx_packets);

uint8_t sample_ringbuf_pop(sample_ringbuffer_t *rbuf, sample_ringbuffer_data_t *dst);

#endif /* __included_ringbuf_h__ */
