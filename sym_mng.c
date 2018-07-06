/*
 * sym_mng.c
 *
 *  Created on: Apr 20, 2018
 *      Author: gasha
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#define BUFSIZE 256

int* pids = NULL;
int pattern_len = 0;
int* parentfd = NULL;

void freeArr(int * arr) {
	if (arr != NULL) {
		free(arr);
	}
}

void sigpipe_handler(int signum) {
	printf("SIGPIPE for Manager process %d. Leaving\n", getpid());
	int i = 0;
	for (i = 0; i < pattern_len; i++) {
		if (pids[i] != -1) {
			kill(pids[i], SIGTERM);
		}
	}
	freeArr(pids);
	freeArr(parentfd);
	exit(1);
}

int sigpipe_register() {
	struct sigaction new_action_mng;
	memset(&new_action_mng, 0, sizeof(new_action_mng));
	new_action_mng.sa_handler = sigpipe_handler;
	return sigaction(SIGPIPE, &new_action_mng, NULL);
}

void noKids(int* pids, int arr_size) {
	for (int i = 0; i < arr_size; i++) {
		if (pids[i] != -1) {
			kill(pids[i], SIGTERM);
		}
	}
}

void closePips(int * pipes, int arr_size) {
	for (int i = 0; i < arr_size; i++) {
		if (pipes[i] != -1) {
			close(pipes[i]);
		}
	}
}

int checkPids(int* pids, int arr_size) {
	for (int i = 0; i < arr_size; i++) {
		if (pids[i] != -1) {
			return 1;
		}
	}
	return 0;
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
	int k = 0;
	int i = 0;
	if (argc < 3) { //assert there are 3 arguments
		printf("Error in number of given args!\n");
		return EXIT_FAILURE;
	}
    if (sigpipe_register() == -1) {
        printf("Signal handle registration failed\n");
        return EXIT_FAILURE;
    }
	pattern_len = strlen(argv[2]);
	pids = malloc(sizeof(int) * pattern_len);
	if (pids == NULL) {
		printf("Memory allocation error\n");
		return EXIT_FAILURE;
	}
	parentfd = malloc(sizeof(int) * pattern_len);
	if (parentfd == NULL) {
		printf("Memory allocation error\n");
		freeArr(pids);
		return EXIT_FAILURE;
	}
	for (k = 0; k < pattern_len; k++) { //init pids
		pids[k] = -1;
		parentfd[k] = -1;
	}
	for (i = 0; i < pattern_len; i++) {
		int pipefd[2];
		int j = 0;
		if (-1 == pipe(pipefd)) {
			printf("Error in pipe! %s \n", strerror(errno));
			return EXIT_FAILURE;
		}
		pid_t pid = fork();
		if (pid == -1) {
			printf("Error in forking! %s \n", strerror(errno));
			for (j = 0; j < i; j++) {
				kill(pids[j], SIGTERM);
			}
			closePips(parentfd, pattern_len);
			noKids(pids, pattern_len);
			freeArr(pids);
			freeArr(parentfd);
			return EXIT_FAILURE;
		}
		if (pid == 0) {  // child
			close(pipefd[0]);
			// reference: https://stackoverflow.com/questions/3068397/finding-the-length-of-an-integer-in-c
			char index[] = { argv[2][i], '\0' };
			int nDigits = getLen(pipefd[1]);
			char child_fd[nDigits];
			sprintf(child_fd, "%d", pipefd[1]);
			char *exec_args[] =
					{ "./sym_count", argv[1], index, child_fd, NULL };
			int check_error = execvp("./sym_count", exec_args);
			if (check_error == -1) {
				printf("Error in execvp! %s \n", strerror(errno));
				for (j = 0; j < i; j++) {
					kill(pids[j], SIGTERM);
				}
				noKids(pids, pattern_len);
				freeArr(pids);
				freeArr(parentfd);
				return EXIT_FAILURE;
			}
		} else { // parent
			close(pipefd[1]);
			pids[i] = pid;
			parentfd[i] = pipefd[0];
		}
	}
	sleep(1);
	char buf[BUFSIZE + 1];
	while (checkPids(pids, pattern_len)) {
		for (int i = 0; i < pattern_len; i++) {
			int status;
			int wait_success = waitpid(pids[i], &status, WNOHANG);
			int j = 0;
			if (wait_success == -1) {
				printf("waitpid failed! %s\n", strerror(errno));
				for (j = 0; j < pattern_len; j++) {
					kill(pids[j], SIGTERM);
				}
				noKids(pids, pattern_len);
				freeArr(pids);
				freeArr(parentfd);
				return EXIT_FAILURE;
			}
			if (WIFEXITED(status)) {
				if (status == 0) {
					int read_bytes;
					while ((read_bytes = read(parentfd[i], buf, BUFSIZE)) > 0) {
						buf[read_bytes] = '\0';
						printf("%s", buf);
					}
					if (read_bytes == -1)
						printf("Failed reading from pipe %s\n",
								strerror(errno));
					close(pids[i]);
					pids[i] = -1;
					buf[0] = '\0';
				}
			} else {
				close(pids[i]);
				pids[i] = -1;
			}
		}
		sleep(1);
	}
	freeArr(pids);
	freeArr(parentfd);
	return EXIT_SUCCESS;
}
