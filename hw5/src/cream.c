#include "cream.h"
#include "utils.h"
#include "hashmap.h"
#include "queue.h"
#include "csapp.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#define UNKNOWN 0x80
#define HELP(prog_name)																\
	do{																				\
	fprintf(stderr,																	\
	"%s [-h] NUM_WORKERS PORT_NUMBER MAX_ENTRIES\n"									\
	"-h                 Displays this help menu and returns EXIT_SUCCESS.\n"		\
	"NUM_WORKERS        The number of worker threads used to service requests.\n"	\
	"PORT_NUMBER        Port number to listen on for incoming connections.\n"		\
	"MAX_ENTRIES        The maximum number of entries that can be stored in `cream`'s underlying data store.\n", (prog_name));												\
	} while(0)

typedef struct map_insert_t {
    void *key_ptr;
    void *val_ptr;
	int key_len;
	int val_len;
} map_insert_t;

static uint32_t NUM_WORKERS;
static char* PORT_NUMBER;
static uint32_t MAX_ENTRIES;

queue_t *requestQueue;
hashmap_t *globalHash;

void parse_args(int argc, char* argv[]) {
	if(argc != 4) goto error;
	for(int i = 1; i < argc; i++) {
		int num;
		if((num = atoi(argv[i])) <= 0)
			goto error;
		if(i == 1) NUM_WORKERS = num;
		else if(i == 2) PORT_NUMBER = argv[i];
		else MAX_ENTRIES = num;
	}
	return;
error:
	HELP(argv[0]);
	exit(EXIT_FAILURE);
}

void queue_free_function(void *item) {
    free(item);
}

void map_free_function(map_key_t key, map_val_t val) {
    free(key.key_base);
    free(val.val_base);
}

static int read_key_val(int* connfd, map_insert_t* insert) {
	if(errno == EPIPE || Rio_readn(*connfd,insert->key_ptr,insert->key_len) != (ssize_t)insert->key_len){
		return -1;
	}
	if(insert->val_len > 0) {
	if(errno == EPIPE || Rio_readn(*connfd,insert->val_ptr,insert->val_len) != (ssize_t)insert->val_len)
		return -1;
	}
	return 0;
}

static uint8_t request_handler(int* connfd, map_insert_t* insert) {
	int err = 0;
	request_codes code;
	size_t size_reqH = sizeof(request_header_t);
	request_header_t* header = malloc(size_reqH);
	if(errno == EPIPE || Rio_readn(*connfd, header, size_reqH) != size_reqH) {
		err = -1;
		goto final;
	}
	else {
		if(header->key_size < MIN_KEY_SIZE || header-> key_size > MAX_KEY_SIZE) {
			if(header->request_code != CLEAR) {
				err = -1;
				goto final;
			}
		}
		insert->key_len = header->key_size;
		insert->key_ptr = malloc(header->key_size);
		code = header->request_code;
		switch(code) {
			case PUT:
				if(header->value_size < MIN_VALUE_SIZE ||
					header->value_size > MAX_VALUE_SIZE) {
					err = -1;
					goto final;
				}
				insert->val_len = header->value_size;
				insert->val_ptr = malloc(header->value_size);
				err = read_key_val(connfd, insert);
				break;
			case GET:
			case EVICT:
			case CLEAR:
				err = read_key_val(connfd, insert);
				break;
			default:
				err = -1;
				goto final;
		}
	}
final:
	free(header);
	return (err < 0) ? UNKNOWN : code;
}

static void response_handler(int* connfd, uint32_t resCode, map_insert_t* insert) {
	size_t size_resH = sizeof(response_header_t);
	response_header_t* header = calloc(1, size_resH);
	header->response_code = resCode;
	switch(resCode) {
		case OK:
			header->value_size = insert->val_len;
			if(errno != EPIPE)
				Rio_writen(*connfd, header, size_resH);
			if(insert->val_len != 0 && errno != EPIPE) {
				Rio_writen(*connfd, insert->val_ptr, insert->val_len);
			}
			break;
		case NOT_FOUND:
		case BAD_REQUEST:
		case UNSUPPORTED:
		default:
			if(errno != EPIPE)
				Rio_writen(*connfd, header, size_resH);
	}
	return;
}

uint32_t hashmap_handler(uint8_t req_code, map_insert_t* insert) {
	response_codes res_code;
	map_val_t val;
	bool success;
	switch(req_code) {
		case PUT:
			success = put(globalHash, MAP_KEY(insert->key_ptr, insert->key_len),
							MAP_VAL(insert->val_ptr, insert->val_len), true);
			res_code = (success) ? OK : BAD_REQUEST;
			insert->val_len = 0;
			break;
		case GET:
			val = get(globalHash, MAP_KEY(insert->key_ptr, insert->key_len));
			if(val.val_base != NULL && val.val_len > 0) {
				insert->val_ptr = val.val_base;
				insert->val_len = val.val_len;
				res_code = OK;
			}
			else 
				res_code = NOT_FOUND;
			break;
		case EVICT:
			delete(globalHash, MAP_KEY(insert->key_ptr, insert->key_len));
			res_code = OK;
			break;
		case CLEAR:
			clear_map(globalHash);
			res_code = OK;
			break;
		case UNKNOWN:
		default:
			res_code = UNSUPPORTED;
	}
	return res_code;
}

void *thread(void *vargp) {
	Pthread_detach(pthread_self());
	signal(SIGPIPE, SIG_IGN);
	while(1) {
		int* connfd = dequeue(requestQueue);
		if(connfd != NULL) {
        	map_insert_t *insert = calloc(1, sizeof(map_insert_t));
			uint8_t reqCode = request_handler(connfd, insert);
			if(errno != EPIPE) {
				response_codes resCode = hashmap_handler(reqCode, insert);
				response_handler(connfd, resCode, insert);
			}
			free(insert);
		}
		close(*connfd);
		free(connfd);
		errno = 0;
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	parse_args(argc, argv);

	int listenfd;
	socklen_t clientlen;
	struct sockaddr clientaddr;
	pthread_t tid;

	listenfd = Open_listenfd(PORT_NUMBER);

	requestQueue = create_queue();
	globalHash = create_map(MAX_ENTRIES, jenkins_one_at_a_time_hash, map_free_function);

	for(int i = 0; i < NUM_WORKERS; i++) {
		Pthread_create(&tid, NULL, thread, NULL);
	}

	while(1) {
		clientlen = sizeof(struct sockaddr);
		int* connfd = malloc(sizeof(int));
		*connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);
		enqueue(requestQueue, connfd);
	}
	
//final:
	invalidate_queue(requestQueue, queue_free_function);
	invalidate_map(globalHash);
	free(requestQueue);
	free(globalHash);
    exit(0);
}


