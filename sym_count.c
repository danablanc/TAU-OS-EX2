/*
 * sym_count.c
 *
 *  Created on: Apr 20, 2018
 *      Author: gasha
 */
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <math.h>
#include <limits.h>

int fd = -1;
char val = '\0';
int counter = 0;
int pipe_child = -1;

void release() {
	if (fd != -1)
		close(fd);
	if (pipe_child != -1)
		close(fd);
}

void sigterm_handler(int signum) {
	release();
	exit(0);
}

void sigpipe_handler(int signum) {
	printf("SIGPIPE for process %d. Symbol %c. Counter %d.Leaving.\n", getpid(),
			val, counter);
	release();
	exit(0);
}

int sigterm_register() {
	struct sigaction new_action_term;
	memset(&new_action_term, 0, sizeof(new_action_term));
	new_action_term.sa_handler = sigterm_handler;
	return sigaction(SIGTERM, &new_action_term, NULL);
}

int sigpipe_register() {
	struct sigaction new_action;
	memset(&new_action, 0, sizeof(new_action));
	new_action.sa_handler = sigpipe_handler;
	return sigaction(SIGPIPE, &new_action, NULL);
}

// reference: https://stackoverflow.com/questions/3068397/finding-the-length-of-an-integer-in-c
int getLen(int n) {
	if (n < 0)
		return getLen((n == INT_MIN) ? INT_MAX : -n);
	if (n < 10)
		return 1;
	return 1 + getLen(n / 10);
}

int main(int argc, char** argv) {
	int i = 0;
	if (argc < 4) { //assert there are 3 arguments
		printf("Error in number of given args!\n");
		return EXIT_FAILURE;
	}
	// used the following reference: https://stackoverflow.com/questions/20460670/reading-a-file-to-string-with-mmap
	struct stat s;
    if (sigterm_register() == -1) {
        printf("Signal handle registration failed\n");
        return EXIT_FAILURE;
    }
    if (sigpipe_register() == -1) {
        printf("Signal handle registration failed\n");
        return EXIT_FAILURE;
    }
	val = argv[2][0];
	pipe_child = atoi(argv[3]);
	fd = open(argv[1], O_RDONLY);
	int status = fstat(fd, &s);
	if (fd < 0) {
		printf("Error opening file: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	if (status == -1) {
		printf("Error in fstat: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	int size = s.st_size;
	unsigned char *f = (char *) mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (*f == -1) {
		printf("Error in mmap: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	for (i = 0; i < size; i++) { // counter
		char c = f[i];
		if (f[i] == val) {
			counter += 1;
		}
	}
	int pidlen = getLen(getpid());
	int counterlen = getLen(counter);
	int total_len = pidlen + counterlen + 43;
	char str[total_len + 1];
	str[total_len] = '\0';
	sprintf(str, "Process %d finishes. Symbol %c. Instances %d.\n", getpid(),
			val, counter);
	write(pipe_child, str, strlen(str));
	if (-1 == munmap(f, size)) {
		printf("Error un-mmapping the file: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	close(pipe_child);
	close(fd);
	return EXIT_SUCCESS;

}

