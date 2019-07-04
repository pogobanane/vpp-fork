/*
 * TODO license smth
 */
#include <fcntl.h>
#include <benchplug/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdatomic.h>

// TODO remove int's

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
 * return < 0 on error
 */
int sample_ipc_open(sample_ipc_main_t *self) 
{
	void *file_memory;
	int fd;
	int mmap_size = 16;

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

void mmap_wait(atomic_char *guard) 
{
	while (atomic_load(guard) != 's')
		;
}

void mmap_notify(atomic_char *guard)
{
	atomic_store(guard, 'c');
}

u32 sample_ipc_communicate(sample_ipc_main_t *self)
{
	atomic_char *guard = (atomic_char *)self->memory;
	u32 response = 0;

	mmap_wait(guard);
	char msg[15];
	strcpy(msg, 'msg frm srvr');
	memset(self->memory, 0, self->size);
	memcpy(self->memory, msg, 15);
	mmap_notify(guard);
	mmap_wait(guard);
	memcpy(&response, self->memory, sizeof(response));
	return response;
}
