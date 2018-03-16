#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

int get_key_value(int argc, char* argv[]){
	int value;
	if(argc < 3 || strcmp(argv[1],"-p") || argc > 3){
		printf("Usage: programm -p N (N > 0)\n");
		exit(2);
	}
	value = atoi(argv[2]);
	return value;
}

int main(int argc, char* argv[])
{
	pid_t pid;
	int i, N;
	int* fds;
	int fds_size;
	
	N = get_key_value(argc, argv);

	fds_size = 2*((N*N)+N);
	fds = (int*)malloc(sizeof(int)*fds_size);
	for(i = 0; i < fds_size; i+=2){
		if(pipe(&fds[i])==-1){
			printf("Failed to create pipe");
		}
		if(close(fds[i])==-1 || close(fds[i+1])==-1){
			printf("Failed to close pipe");
		}
	}

	for(i = 0; i < N; i++){
		switch(pid = fork()){
			case -1:
				perror("fork");
				break;
			case 0:
				printf("child -- PID = %d\n", getpid());
				printf("ready to exit\n");
				exit(0);
				break;
			default:

				break;
		}
	}

	for(i = 0; i < N; i++){
		wait(NULL);
	}

	return 0;
}
