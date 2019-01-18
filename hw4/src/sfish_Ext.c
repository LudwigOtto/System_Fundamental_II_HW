#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include "sfish_Ext.h"
#include "sfish_signal.h"
#include "sfish.h"

char oldPWD[BUFFER_SIZE] = {0};
int pgid[MAX_NUM_PGID] = {0};
int* fdPipeList;
int* pipeFuncList;
static int numPipe = 0;
static int idxPgid = 0;
volatile int totalPgid = 1;

SNode* createSNode(int val) {
	SNode* head = (SNode*)malloc(sizeof(SNode));
	head->val = val;
	head->next = NULL;
	return head;
}

SNode* addSNode(int val, SNode** head) {
	SNode* node = createSNode(val);
	if(*head == NULL)
		*head = node;
	else {
		SNode* ptr = *head;
		while(ptr->next != NULL) {
			ptr = ptr->next;
		}
		ptr->next = node;
	}
	return *head;
}

int peakSNode(SNode* node) {
	if(node == NULL)
		return -1;
	else {
		int val = node->val;
		return val;
	}
}

void removeSNode(SNode** head) {
	if(*head == NULL) {
		return;
	}
	SNode* ptr = *head;
	while(ptr->next != NULL) {
		SNode* next = ptr->next;
		free(ptr);
		ptr = next;
	}
	free(ptr);
	*head = NULL;
	return;
}

int setRedirection(char** tokens, int idxRdr, int* fd) {
	char redirector = **(tokens+idxRdr);
	if(redirector == '>') {
		if((*fd = open(tokens[idxRdr + 1], O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR)) < 0)
			goto err;
		dup2(*fd,STDOUT_FILENO);
	}
	else {
		if((*fd = open(tokens[idxRdr + 1], O_RDONLY)) < 0)
			goto err;
		dup2(*fd,STDIN_FILENO);
	}
	return *fd;
err:
	fprintf(stderr, SYNTAX_ERROR, strerror(errno));
	return -1;
}

int setPipe(char** tokens, int idxRdx, int* fd) {
	if(numPipe == 0)
		fdPipeList = (int*)malloc(TOKEN_SIZE * sizeof(int));
	fdPipeList[numPipe * 2] = *fd;
	fdPipeList[numPipe * 2+ 1] = *(fd + 1);

	pipe(fdPipeList + (numPipe * 2));

	if(numPipe == 0) {
		pipeFuncList = (int*)malloc(TOKEN_SIZE * sizeof(int));
		pipeFuncList[0] = 0;
	}
	numPipe += 1;
//err:
	//fprintf(stderr, EXEC_ERROR, strerror(errno));
	//return -1;
	return 0;
}

void removeRdrFromTokens(char** tokens, SNode** redirList) {
	SNode* node = *redirList;
	int shift = 0;
	while(node != NULL) {
		int idx = node->val - shift;
		if(strcmp(tokens[idx], ">") == 0 || strcmp(tokens[idx], "<") == 0)
			shift += 2;
		while(tokens[idx] != NULL) {
			tokens[idx] = tokens[idx + 2];
			tokens[idx + 1] = tokens[idx + 2 + 1];
			idx += 2;
		}
		node = node->next;
	}
	return;
}

void changePipeInTokens(char** tokens) {
	char** ptr = tokens;
	int i = 1;
	while(*ptr != NULL) {
		if(strcmp(*ptr, "|") == 0) {
			pipeFuncList[i] = ptr - tokens + 1;
			*ptr = NULL;
			i++;
		}
		ptr++;
	}
	pipeFuncList[i] = -1;
	return;
}

void saveStdIO(int* savedOut, int* savedIn) {
	*savedOut = dup(STDOUT_FILENO);
	*savedIn = dup(STDIN_FILENO);
	return;
}

void restoreStdIO(int* savedOut, int* savedIn) {
	dup2(*savedOut, STDOUT_FILENO);
	dup2(*savedIn, STDIN_FILENO);
	return;
}

void dropFd(int* fd, int numFd) {
	for(int i = 0; i< numFd; i++) {
		if(*(fd + i) > 0)
			close(*(fd + i));
	}
	free(fd);
	return;
}

void clearPipe() {
	if(pipeFuncList != NULL)
		free(pipeFuncList);
	if(fdPipeList != NULL)
		free(fdPipeList);
	numPipe = 0;
	return;
}

int builtInCd(char** tokens, char* curPath) {
	int ret;
	if(strcmp(*(tokens + 1), "-") == 0) {
		write(1, oldPWD, strlen(oldPWD));
		write(1, "\n", 1);
		if(strlen(oldPWD) != 0)
			ret = chdir(oldPWD);
		strcpy(oldPWD, curPath);
	}
	else {
		strcpy(oldPWD, curPath);
		char nextPath[BUFFER_SIZE];
		char* ptr;
		if((ptr = strstr(*(tokens + 1), "~")) != NULL) {
			strcpy(nextPath, HOMEBUF);
			strcat(nextPath, ptr + 1);
		}
		else
			strcpy(nextPath, *(tokens + 1));
		ret = chdir(nextPath);
	}
	return ret;
}

int builtInPwd(char* curPath) {	
	write(1, curPath, strlen(curPath));
	write(1, "\n", 1);
	return 0;
}

int builtInHelp() {
	write(1, " cd [-L|[-P [-e]] [-@]] [dir]\n", strlen(" cd [-L|[-P [-e]] [-@]] [dir]\n"));
	write(1, " exit [n]\n", strlen(" exit [n]\n"));
	write(1, " help [-dms] [pattern ...]\n", strlen(" help [-dms] [pattern ...]\n"));
	write(1, " pwd [-LP]\n", strlen(" pwd [-LP]\n"));
	return 0;
}


int isBuiltIn(char** tokens, char* curPath) {
	char* func = *tokens;
	if(func == NULL)
		return 0;
	if(strcmp(func, "help") == 0) {
		builtInHelp();
		return 1;
	}
	if(strcmp(func, "cd") == 0) {
		if(builtInCd(tokens, curPath) != 0)
			fprintf(stderr, BUILTIN_ERROR, strerror(errno));
		return 1;
	}
	if(strcmp(func, "pwd") == 0) {
		builtInPwd(curPath);
		return 1;
	}
	if(strcmp(func, "exit") == 0) {
		free(tokens);
		free(curPath);
		exit(EXIT_SUCCESS);
	}
	return 0;
}

void executable(char* path, char** argv) {
	pgid[idxPgid++] = getpid();
	sigprocmask(SIG_BLOCK, &sigMask, &sigPrev);
	int cpid = fork();
	if(cpid == 0) {
		kill(pgid[0], SIGTSTP);
		if(execvp(path, argv) != 0) {
			if(errno == ENOENT)
				fprintf(stderr, EXEC_NOT_FOUND, path);
			else
				fprintf(stderr, EXEC_ERROR,strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	else {
		setpgid(cpid, 0);
		pgid[idxPgid++] = getpgid(cpid);
		totalPgid++;
		atPid = 0;
		while(!atPid)
			sigsuspend(&sigPrev);

		sigprocmask(SIG_SETMASK, &sigPrev, NULL);
		idxPgid = 0;
	}
	return;
}

void runPipeInput(char** tokens, char* curPath) {
	int childStatus;
	int* ptrFd = fdPipeList;
	int* ptrFunc = pipeFuncList;
	int pid;
	int fd_in = dup(STDIN_FILENO);
	int fd_out = dup(STDOUT_FILENO);
	int loop = numPipe;
	int pip[2];
	int ref[2];
	pipe(ref);

	while(loop > 0) {
		pip[0] = *ptrFd;
		pip[1] = *(ptrFd + 1);

		if((pid = fork()) == 0) {
			if(loop < numPipe) {
				dup2(ref[0], STDIN_FILENO);
			}

			if(*(ptrFunc + 1) != -1) {
				dup2(pip[1], STDOUT_FILENO);
			}
			else {
				dup2(fd_out, STDOUT_FILENO);
			}

			if(!isBuiltIn((tokens + *(ptrFunc)), curPath))
				execvp(tokens[*(ptrFunc)], (tokens + *(ptrFunc)));
			close(pip[0]);
			exit(EXIT_FAILURE);
		}
		else {
			if(waitpid(pid, &childStatus, WUNTRACED) == -1)
				fprintf(stderr, EXEC_ERROR, strerror(errno));
			close(pip[1]);
			dup2(pip[0], STDIN_FILENO);

			if(loop > 1) {
				dup2(ref[1] , STDOUT_FILENO);
			}
			else {
				dup2(fd_out, STDOUT_FILENO);
			}

			if(*(ptrFunc + 1) != -1) {
				if(!isBuiltIn((tokens + *(ptrFunc + 1)), curPath))
					executable(tokens[*(ptrFunc+1)], (tokens + *(ptrFunc+1)));
			}

			ptrFunc += 2;
			ptrFd += 2;
			loop -= 1;

			if(loop == 0) {
				dup2(fd_in, STDIN_FILENO);
			}
		}
	}
	return;
}

