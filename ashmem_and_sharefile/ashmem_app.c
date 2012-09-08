#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdio.h>
#include <ncurses.h>

#include "ashmem.h"
#include "share_file.h"

#define ASHMEM_DEVICE	"/dev/ashmem"
#define SHFILE_DEVICE   "/dev/shfile"
#define NAME		"ashmem_test"
#define LENGTH		4096

int ashmem_open(const char *name, size_t size)
{
	int fd, ret;
	char buf[ASHMEM_NAME_LEN];
	
	fd = open(ASHMEM_DEVICE, O_RDWR);
	if (fd < 0)
		return fd;

	strncpy(buf, name, sizeof(name) + 1);
	ret = ioctl(fd, ASHMEM_SET_NAME, buf);
	if (ret < 0)
		goto error;
	
	ret = ioctl(fd, ASHMEM_SET_SIZE, size);
	if (ret < 0)
		goto error;

	return fd;

error:
	close(fd);
	return ret;
}

int ashmem_pin_region(int fd, size_t offset, size_t len)
{
	struct ashmem_pin pin = { offset, len };
	return ioctl(fd, ASHMEM_PIN, &pin);
}

int ashmem_unpin_region(int fd, size_t offset, size_t len)
{
	struct ashmem_pin pin = { offset, len };
	return ioctl(fd, ASHMEM_UNPIN, &pin);
}

int ashmem_read_bytes(int fd, unsigned int *address, unsigned char *buffer, int src_offset, int dest_offset, int count)
{
	if (ashmem_pin_region(fd, 0, 0) == ASHMEM_WAS_PURGED)
	{
		printf("ashmem was purged\n");
		return -1;
	}
	printf("%s: address=0x%x, buffer=0x%x\n", __FUNCTION__,  (unsigned int)address, (unsigned int)buffer);
	memcpy(buffer + src_offset, (unsigned char *)address + dest_offset, count);
	
	return count;
}

int ashmem_write_bytes(int fd, unsigned int *address, unsigned char *buffer, int src_offset, int dest_offset, int count)
{
	if (ashmem_pin_region(fd, 0, 0) == ASHMEM_WAS_PURGED)
	{
		printf("ashmem was purged\n");
		return -1;
	}

	memcpy((unsigned char *)address + dest_offset, buffer + src_offset, count);

	return count;
}

int ashmem_set_fd(int fd_shmem)
{
	int fd, ret;
	
	fd = open(SHFILE_DEVICE, O_RDWR);
	if (fd < 0)
		return fd;

	ret = ioctl(fd, SHFILE_SHARE_FD, &fd_shmem);
	if (ret < 0)
		return ret;

	close(fd);

	return 0;
}

int ashmem_get_fd(void)
{
	int fd, ret;
	int fd_shmem;
	
	fd = open(SHFILE_DEVICE, O_RDWR);
	if (fd < 0)
	{
		printf("failed to open shfile device\n");
		return fd;
	}

	ret = ioctl(fd, SHFILE_GET_FD, &fd_shmem);
	if (ret < 0)
	{
		printf("failed to get shmem fd\n");
		return ret;
	}
	close(fd);

	printf("get shmem fd=%d\n", fd_shmem);
	return fd_shmem;
}	

int test_ashmem(void)
{
	int		 fd, ret;
	unsigned int	*mAddress;
	unsigned char	*buf  = "ashmem test";
	unsigned char	 read_buf[30];
	unsigned int	 size =	0;

	fd = ashmem_open(NAME, LENGTH);
	if (fd < 0)
		printf("open ashmem error\n");

	size = ioctl(fd, ASHMEM_GET_SIZE, NULL);
	printf("size=%d\n", size);
	
	mAddress = (unsigned int *)mmap(NULL, LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mAddress < 0)
		printf("mmap failed\n");

	ret = ashmem_write_bytes(fd, mAddress, buf, 0, 0, strlen(buf) + 1);
	if (ret < 0)
	 	printf("write failed\n");

	ashmem_read_bytes(fd, mAddress, read_buf, 0, 0, strlen(buf) + 1); 
	if (ret < 0) 
	 	printf("read failed"); 
	printf("read data: %s\n", read_buf);

	ashmem_unpin_region(fd, 0, 0); 

	ret = munmap((void *)mAddress, LENGTH);
	if (ret < 0)
		printf("unmap failed\n");
	
	close(fd);

	return ret;
}


int shfile_server(void)
{
	int		 fd, ret;
	unsigned int	*mAddress;
	unsigned char	*buf  = "ashmem test";
	unsigned char	 read_buf[30];
	unsigned int	 size =	0;

	fd = ashmem_open(NAME, LENGTH);
	if (fd < 0)
		printf("open ashmem error\n");

	size = ioctl(fd, ASHMEM_GET_SIZE, NULL);
	printf("size=%d\n", size);
	
	mAddress = (unsigned int *)mmap(NULL, LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mAddress < 0)
		printf("mmap failed\n");

	ret = ashmem_write_bytes(fd, mAddress, buf, 0, 0, strlen(buf) + 1);
	if (ret < 0)
	 	printf("write failed\n");

	ashmem_read_bytes(fd, mAddress, read_buf, 0, 0, strlen(buf) + 1); 
	if (ret < 0) 
	 	printf("read failed"); 
	printf("read data: %s\n", read_buf);

	ashmem_set_fd(fd);

	int key;
	initscr();
	cbreak();
	keypad(stdscr, TRUE);
	noecho();
	timeout(1);

	clear();
	mvprintw(5,5, "server running:");

	while (1)
	{
		int i;
	
		move(7,0);
		clrtoeol();
		refresh();
	
		for (i = 0; i < 80; i++)
		{
			mvprintw(7, i, ".");
			refresh();
			usleep(50000);
			key = getch();
			if (key == 'q')
				break;
		}
		if (i < 80)
			break;

	}
	endwin();

	ashmem_unpin_region(fd, 0, 0); 

	ret = munmap((void *)mAddress, LENGTH);
	if (ret < 0)
		printf("unmap failed\n");
	
	close(fd);

	return ret;
}

int shfile_client(void)
{
	int		 fd, ret;
	unsigned int	*mAddress;
	unsigned char	*buf  = "ashmem test";
	unsigned char	 read_buf[30];
	unsigned int	 size =	0;

	fd = ashmem_get_fd();

	size = ioctl(fd, ASHMEM_GET_SIZE, NULL);
	printf("size=%d\n", size);
	
	mAddress = (unsigned int *)mmap(NULL, LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (mAddress < 0)
		printf("mmap failed\n");

	ashmem_read_bytes(fd, mAddress, read_buf, 0, 0, strlen(buf) + 1); 
	if (ret < 0) 
	 	printf("read failed"); 
	printf("read data: %s\n", read_buf);


	ret = munmap((void *)mAddress, LENGTH);
	if (ret < 0)
		printf("unmap failed\n");

	printf("shfile client closed\n");
	
	return ret;
}


int main(int argc, char *argv[])
{

	if (argc == 1)
	{
		printf("run shmem test\n");
		test_ashmem();
	}
	else if (argc == 2 && !strcmp(argv[1], "s"))
	{
		printf("run shfile server\n");
		shfile_server();
	}
	else if (argc == 2 && !strcmp(argv[1], "c"))
	{
		printf("run shfile client\n");
		shfile_client();
	}
	else
		printf("error argument\n");

	return 0;
			
}
