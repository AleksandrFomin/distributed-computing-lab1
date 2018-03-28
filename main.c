#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

typedef struct {
	int*** matrix;
	local_id proc_id;
} SourceProc;

int get_key_value(int argc, char* argv[]){
	int value;
	if(argc < 3 || strcmp(argv[1],"-p") || argc > 3){
		printf("Usage: programm -p N (N > 0)\n");
		exit(2);
	}
	value = atoi(argv[2]);
	return value;
}

int*** create_matrix(int N){
	int *** matrix;
	int i,j;
	int fd[2];
	matrix = (int***)malloc(sizeof(int**)*(N+1));
	for(i = 0; i <= N; i++){
		matrix[i] = (int**)malloc(sizeof(int*)*(N+1));
		for(j = 0; j <= N; j++){
			matrix[i][j]=(int*)malloc(sizeof(int)*2);
			if(i!=j){
				if(pipe(fd)==-1){
					printf("Failed to create pipe");
				}
				matrix[i][j][0] = fd[0];
				matrix[i][j][1] = fd[1];
			}
		}
	}
	return matrix;
}

int close_pipes(int*** matrix, int N, int num){
	int j,k;
	for(j = 0; j <= N; j++){
		for(k = 0; k <= N; k++){
			if(j==k){
				continue;
			}
			if(j==num){
				if(close(matrix[j][k][0])<0){
					return -1;
				}
				continue;
			}
			if(k==num){
				if(close(matrix[j][k][1])<0){
					return -1;
				}
				continue;
			}
			if(close(matrix[j][k][0])<0){
				return -1;
			}
			if(close(matrix[j][k][1])<0){
				return -1;
			}
		}
	}
	return 0;
}

int write_to_log(int*** matrix, int N){
	int fd, i, j;
	char str[100];
	if((fd = open(pipes_log, O_WRONLY)) == -1){
		return -1;
	}
	for(i = 0; i <= N; i++){
		for(j = 0; j <= N; j++){
			if(i!=j){
				sprintf(str,"Pipe %d - %d. fds: %d & %d\n",
					i, j, matrix[i][j][0], matrix[i][j][1]);
				if(write(fd, str, strlen(str))<0){
					return -1;
				}
			}
		}
	}
	return 0;
}

int send(void * self, local_id dst, const Message * msg){
	int fd, proc_from;
	int*** matrix;
	SourceProc* sp;

	sp = (SourceProc*)self;
	matrix = sp->matrix;
	proc_from = sp->proc_id;
	fd = matrix[proc_from][dst][1];

	if(write(fd, msg, sizeof(MessageHeader) + 
		msg->s_header.s_payload_len) < 0){
		return -1;
	}
	return 0;
}

int send_multicast(void * self, const Message * msg){
	
}

int main(int argc, char* argv[])
{
	pid_t pid;
	int i, N;
	int*** fds;
	local_id proc_id;

	N = get_key_value(argc, argv);
	fds = create_matrix(N);

	if(write_to_log(fds, N) == -1){
		printf("Error writing to log file");
	}
	
	for(i = 0; i < N; i++){
		switch(pid = fork()){
			case -1:
				perror("fork");
				break;
			case 0:
			proc_id = i + 1;
				if(close_pipes(fds, N, proc_id) < 0){
					printf("Error closing pipe");
				}
				exit(0);
				break;
			default:
				break;
		}
	}
	if(close_pipes(fds, N, PARENT_ID) < 0){
		printf("Error closing pipe");
	}

	for(i = 0; i < N; i++){
		wait(NULL);
	}

	return 0;
}
