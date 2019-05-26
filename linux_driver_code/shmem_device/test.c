#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	const char data[] = "test data";
	char buf[128];
	int fd = open("/dev/smdev/smdev0", O_RDWR);
	if (fd == -1) {
		printf("open smdev0 failed\n");
		return -1;
	}
	
	if ((argc > 1) && !strcmp(argv[1], "w")) {
		write(fd, data, strlen(data) + 1);
	}

	memset(buf, 0, sizeof(buf));
	read(fd, buf, sizeof(buf));
	printf("read file: %s\n", buf);	

	//wait
	fgets(buf, sizeof(buf), stdin);

	close(fd);
	return 0;
}
