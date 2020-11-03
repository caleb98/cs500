#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) 
{
	if(argc < 2) {
		printf("Specify read or write with \'w\' or \'r\'.\n");
		return 0;
	}

	int fd = open("/dev/pmod", O_RDWR);

	if(fd < 0) {
		printf("Error getting file descriptor.\n");
		return -1;
	}

	if(strcmp(argv[1], "w") == 0) {

		// Write
		if(argc < 3) {
			printf("Specify a string to write!\n");
		}
		else {
			printf("Writing:\n");
			char out[ strlen(argv[2]) + 2 ];
			strcpy(out, argv[2]);
			strcat(out, "\n");
			printf("%s", out);
			write(fd, out, strlen(out));
		}

	}
	else if(strcmp(argv[1], "r") == 0) {

		// Read
		printf("Reading:\n");
		char buf[8];
		while(read(fd, buf, 8) > 0) {
			printf("%s", buf);
		}

	}
	else {
		printf("Invalid flags. Use \'w\' or \'r\'.\n");
	}

	printf("Closing file.\n");
	close(fd);

	return 0;

}