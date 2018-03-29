#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include "common.h"
#include "ipc.h"
#include "pa1.h"

typedef struct {
	int*** matrix;
	local_id proc_id;
	int N;
	int fd;
} SourceProc;

int get_key_value(int argc, char* argv[]);
int*** create_matrix(int N);
int close_pipes(int*** matrix, int N, int num);
int log_pipes(int*** matrix, int N);
int log_events(int fd, char* str);
SourceProc* prepare_source_proc(int*** matrix, int proc_id,
	int N, int fd);
MessageHeader prepare_message_header(uint16_t len, int16_t type);
Message* prepare_message(MessageHeader msg_header, char msg_text[MAX_PAYLOAD_LEN]);
int send_message(int*** matrix, local_id proc_id, int N, 
		int fd, MessageType type);
int get_message(int*** matrix, local_id proc_id, int N, 
		int fd, MessageType type);
int first_phase(int*** matrix, int proc_id, int N, int fd);

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

int log_pipes(int*** matrix, int N){
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

int log_events(int fd, char* str){
	if(write(fd, str, strlen(str)) < 0){
		return -1;
	}
	printf("%s", str);
	return 0;
}

int send(void * self, local_id dst, const Message * msg){
	int pipe_fd, proc_from;
	int*** matrix;
	SourceProc* sp;

	sp = (SourceProc*)self;
	matrix = sp->matrix;
	proc_from = sp->proc_id;
	pipe_fd = matrix[proc_from][dst][1];

	if(write(pipe_fd, msg, sizeof(MessageHeader) + 
		msg->s_header.s_payload_len) < 0){
		return -1;
	}
	return 0;
}

int send_multicast(void * self, const Message * msg){
	int proc_from, i, N;
	SourceProc* sp;

	sp = (SourceProc*)self;
	N = sp->N;
	proc_from = sp->proc_id;

	for(i = 0; i <= N; i++){
		if(i == proc_from){
			continue;
		}
		log_events(sp->fd, (char*)msg->s_payload);
		if(send(self, i, msg) < 0){
			return -1;
		}
	}
	return 0;
}

SourceProc* prepare_source_proc(int*** matrix, int proc_id,
	int N, int fd){
	SourceProc* sp = (SourceProc*)malloc(sizeof(SourceProc));
	sp->matrix = matrix;
	sp->proc_id = proc_id;
	sp->N = N;
	sp->fd = fd;
	return sp;
}

MessageHeader prepare_message_header(uint16_t len, int16_t type){
	MessageHeader msg_header;
	msg_header.s_magic = MESSAGE_MAGIC;
	msg_header.s_payload_len = len;
	msg_header.s_type = type;
	msg_header.s_local_time = time(NULL);
	return msg_header;
}

Message* prepare_message(MessageHeader msg_header, char msg_text[MAX_PAYLOAD_LEN]){
	Message* msg = (Message*)malloc(sizeof(Message));
	msg->s_header = msg_header;
	memcpy(msg->s_payload,msg_text,strlen(msg_text));
	// msg->s_payload = msg_text;
	return msg;
}

int send_message(int*** matrix, local_id proc_id, int N, 
		int fd, MessageType type){
	SourceProc* sp;
	Message* msg;
	MessageHeader msg_header;
	char msg_text[MAX_PAYLOAD_LEN];

	sp = prepare_source_proc(matrix, proc_id, N, fd);
	switch(type){
		case STARTED:
			sprintf(msg_text, log_started_fmt, proc_id, getpid(), getppid());
			break;
		case DONE:
			sprintf(msg_text, log_done_fmt, proc_id);
			break;
		default:
			break;
	}
	msg_header = prepare_message_header(strlen(msg_text), 
		type);
	msg = prepare_message(msg_header,msg_text);
	if(send_multicast((void*)sp, msg) < 0){
		return -1;
	}
	return 0;
}

int first_phase(int*** matrix, int proc_id, int N, int fd){
	if(send_message(matrix, proc_id, N, fd, STARTED) < 0){
		return -1;
	}
	if(get_message(matrix, proc_id, N, fd, STARTED) < 0){
		return -1;
	}
	return 0;
}

int receive(void * self, local_id from, Message * msg){
	SourceProc* sp;
	int pipe_fd;
	int*** matrix;
	local_id proc_id;

	sp = (SourceProc*)self;
	proc_id = sp->proc_id;
	matrix = sp->matrix;
	pipe_fd = matrix[from][proc_id][0];
	if(read(pipe_fd, msg, sizeof(Message)) < 0){
		return -1;
	}
	return 0;
}

int receive_all(void* self, MessageType type){
	Message * msg;
	SourceProc* sp;
	int*** matrix;
	int i;
	local_id proc_id;
	char str[MAX_PAYLOAD_LEN];

	msg = (Message*)malloc(sizeof(Message));
	sp = (SourceProc*)self;
	proc_id = sp->proc_id;
	matrix = sp->matrix;
	for(i = 1; i <= sp->N; i++){
		if(i==proc_id){
			continue;
		}
		if(receive(self, i, msg) < 0){
			return -1;
		}
		if(msg->s_header.s_type != type){
			return -1;
		}
	}
	switch(type){
		case STARTED:
			sprintf(str, log_received_all_started_fmt, proc_id);
			break;
		case DONE:
		sprintf(str, log_received_all_done_fmt, proc_id);
			break;
		default:
			break;
	}
	log_events(sp->fd, (char*)msg->s_payload);
	if(write(sp->fd, str, strlen(str)) < 0){
		return -1;
	}
	printf("%s", str);
	return 0;
}

int get_message(int*** matrix, local_id proc_id, int N, 
		int fd, MessageType type){
	SourceProc* sp;

	sp = prepare_source_proc(matrix, proc_id, N, fd);
	if(receive_all(sp, type) < 0){
		return -1;
	}
	return 0;
}



int third_phase(int*** matrix, int proc_id, int N, int fd){
	if(send_message(matrix, proc_id, N, fd, DONE) < 0){
		return -1;
	}
	if(get_message(matrix, proc_id, N, fd, DONE) < 0){
		return -1;
	}
	return 0;
}

int main(int argc, char* argv[])
{
	pid_t pid;
	int i, N, log_fd;
	int*** fds;
	local_id proc_id;

	N = get_key_value(argc, argv);
	fds = create_matrix(N);

	if(log_pipes(fds, N) == -1){
		printf("Error writing to log file");
	}

	if((log_fd = open(events_log, O_WRONLY)) == -1){
		return -1;
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
				if(first_phase(fds, proc_id, N, log_fd) < 0){
					printf("First phase failed");
				}
				// sleep(1);
				if(third_phase(fds, proc_id, N, log_fd) < 0){
					printf("Third phase failed");
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
