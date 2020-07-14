#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BUF_LEN 128

int main(int argc, char *argv[])
{
	int fd;
	char w_buf[BUF_LEN] = "hello";
	char r_buf[BUF_LEN];
	ssize_t s;
	int ret;

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		printf("open device %s failed\n", argv[1]);
		return -1;
	}

	s = write(fd, w_buf, BUF_LEN);
	if (s != BUF_LEN) {
		fprintf(stderr, "write failed, return %ld\n", s);
		ret = -1;
		goto err;
	}

	memset(r_buf, 0, BUF_LEN);
	s = read(fd, r_buf, strlen(w_buf) + 1);
	if (s == -1) {
		fprintf(stderr, "read data failed\n");
		ret = -1;
		goto err;
	}

	fprintf(stderr, "read data(%ld): %s\n", s, r_buf);
	ret = 0;
 
 err:
	close(fd);
	return ret;
}
