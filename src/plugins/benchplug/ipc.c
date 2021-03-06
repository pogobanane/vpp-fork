/*
 * TODO license smth
 */
#include <fcntl.h>
#include <benchplug/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdatomic.h>

// TODO remove int's

struct sample_ipc_mem_t {
	atomic_char guard;
	sample_ipc_for_client_t response;
	sample_ringbuffer_t request;
};

/*
 * An algorithm using sample_ipc_for_client_t would require per queue data which is not available right now in vpp.
 * It is required because udelays etc must only happen if the fastest
 * queue processed by this thread is bored enough to endure udelays.
 *
 * Therefore the following workaround is made for SAMPLE_IPC_RX_QUEUES queues and 
 * tries not to crash on configurations with more than two queues.
 */
uint16_t sample_ipc_queue_min_level(sample_ipc_main_t *self) {
    uint16_t min_level = self->rx_queue[0].reach_level;
    for (uint16_t i = 1; i < SAMPLE_IPC_RX_QUEUES; ++i) {
        if (self->rx_queue[i].reach_level < min_level)
            min_level = self->rx_queue[i].reach_level;
    }
    return min_level;
}

void sample_ipc_queue_set_level(sample_ipc_main_t *self, uint16_t queue_id, uint16_t level) {
    self->rx_queue[queue_id % SAMPLE_IPC_RX_QUEUES].reach_level = level;
}

void mmap_release(atomic_char *guard);

int make_space(int file_descriptor, int bytes) 
{
	lseek(file_descriptor, bytes + 1, SEEK_SET);
	if (write(file_descriptor, "", 1) < 1) {
		return -1; // (0, "Error writing a single goddman byte to file. A single byte!!!!!");
	}
	lseek(file_descriptor, 0, SEEK_SET);
	return 0;
}

/*
 * returns < 0 on error
 */
int get_file_descriptor(int bytes) 
{
	// Open a new file descriptor, creating the file if it does not exist
	// 0666 = read + write access for user, group and world
	int file_descriptor = open("/tmp/mmap", O_RDWR | O_CREAT, 0666);

	if (file_descriptor < 0) {
		return -2; // (0, "Error opening /tmp/mmap"));
	}

	// Ensure that the file will hold enough space
	int ret = make_space(file_descriptor, bytes);
	if ( ret < 0 ) {
		return ret;
	}

	return file_descriptor;
}

/*
 * creates sample_ipc_mem_t as shared memory and stores it's 
 * information and a pointer to it in self
 * return < 0 on error
 */
int sample_ipc_open(sample_ipc_main_t *self) 
{
	void *file_memory;
	int fd;
	struct sample_ipc_mem_t m;
	int mmap_size = sizeof(m);

	fd = get_file_descriptor(mmap_size);
	if (fd < 0) {
		return fd;
	}

	file_memory = mmap(NULL, 1 + mmap_size, PROT_READ | PROT_WRITE, 
			MAP_SHARED, fd, 0);

	if (file_memory < 0) {
		return -3;
	}
	
	self->fd = fd;
	self->memory = file_memory;
	self->size = mmap_size;

	// initialize with sane defaults
	self->last_response.poll1 = 32;
	self->last_response.udelay = 0;
	self->last_response.poll2 = 0;
	self->last_response.usleep = 0;
	self->last_response.poll3 = 0;
	self->last_response.use_interrupt = 0;
	self->last_response.poll4 = 0;

	// initialize
	sample_ringbuf_init(&self->memory->request);
	atomic_char *guard = &(self->memory->guard);
	mmap_release(guard); // #c

	// we don't need fd anymore
	if (close(fd) < 0) {
		return -4; // error closing file
	}

	return 0;
}

clib_error_t* sample_ipc_close(sample_ipc_main_t *self)
{
	if (munmap(self->memory, self->size) < 0) {
		return clib_error_return(0, "Could not free mmap");
	}

	close(self->fd);

	return 0;
}

/*
 + client waits for server to release lock
 */
void mmap_client_wait(atomic_char *guard) 
{
	while (atomic_load(guard) != 'n' && atomic_load(guard) != 's')
		;
}

/*
 + server waits for client to release lock
 */
void mmap_server_wait(atomic_char *guard) 
{
	while (atomic_load(guard) != 'n' && atomic_load(guard) != 'c')
		;
}

/*
 * client takes lock read/write lock
 */
void mmap_client_take(atomic_char *guard)
{
	atomic_store(guard, 'c');
}

/*
 * server takes lock read/write lock
 */
void mmap_server_take(atomic_char *guard)
{
	atomic_store(guard, 's');
}

/*
 * release locks
 */
void mmap_release(atomic_char *guard)
{
	atomic_store(guard, 'n');
}

void sample_ipc_communicate_to_server_prefetch(sample_ipc_main_t *self)
{
	sample_ringbuf_push_prefetch(&self->memory->request);
}

/*
 * client: writes n_rx_packets to server
 * copies response o self->last_response
 * TODO: bufs size is not checked
 */ 
void sample_ipc_communicate_to_server(sample_ipc_main_t *self, uint16_t port_id, uint16_t queue_id, uint32_t n_rx_packets)
{
	atomic_char *guard = &(self->memory->guard);

	mmap_client_wait(guard);
	mmap_client_take(guard); // c
	sample_ringbuf_push(&(self->memory->request), port_id, queue_id, n_rx_packets);
	memcpy(&(self->last_response), &(self->memory->response), sizeof(self->last_response));
	mmap_release(guard); // n
}

/*
 * server: reads and resets the filled ringbuffer from client, copies
 * it to request and sends response
 */ 
void sample_ipc_communicate_to_client(sample_ipc_main_t *self, sample_ipc_for_client_t *response, sample_ringbuffer_t* request)
{
	atomic_char *guard = &(self->memory->guard);

	mmap_server_wait(guard); 
	mmap_server_take(guard); // s
	
	// export ringbuffer to non-ipc memory to parse it there without blocking 
	// the shared memory
	memcpy(request, &(self->memory->request), sizeof(sample_ringbuffer_t));
	// empty the shared memory ringbuffer
	sample_ringbuf_reset(&(self->memory->request));

	memcpy(&(self->memory->response), response, sizeof(sample_ipc_for_client_t));
	mmap_release(guard); // n
	// mmap_wait_for_server(guard);
	// memcpy(&response, self->memory, sizeof(response));
	// return response;
}

