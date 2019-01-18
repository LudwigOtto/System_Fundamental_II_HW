#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <readline/readline.h>
#include <sys/stat.h>
#include <errno.h>
#include "sfish.h"
#include "debug.h"
#include "sfish_Ext.h"
#include <signal.h>
#include "sfish_signal.h"

static SNode** redirList;
static int numFd = 0;
static bool redirFlag = false;
static bool pipeFlag = false;

static char* alterTilde(char* token, char* ptr) {
	char* newToken = (char*)calloc(BUFFER_SIZE, sizeof(char));
	if(ptr != token)
		strncpy(newToken, token, ptr - token);
	strcat(newToken, HOMEBUF);
	if(ptr - token < strlen(token) - 1)
		strcat(newToken, ptr + 1);
	return newToken;
}

static char** splitLineIntoArgs(char* src) {
	int tokenSize = TOKEN_SIZE;
	char** tokens = malloc(tokenSize * sizeof(char*));
	char* token;
	if (!tokens) {
		perror("allocation error");
		exit(EXIT_FAILURE);
	}
	token = strtok(src, DELIMITERS);
	int i = 0;
	while(token != NULL) {
		char *ptr = NULL;
		if((ptr = strstr(token, "~")) != NULL) {
			tokens[i] = alterTilde(token, ptr);
			ptr = NULL;
		}
		else
			tokens[i] = token;
		i++;

		if (i >= tokenSize) {
			tokenSize += TOKEN_SIZE;
			tokens = realloc(tokens, tokenSize * sizeof(char*));
			if (!tokens) {
				perror("allocation error");
				exit(EXIT_FAILURE);
			}
		}

		if((ptr = strstr(REDIRECTORS, token)) != NULL) {
			if(*redirList == NULL)
				*redirList = createSNode(i -1);
			else
				*redirList = addSNode(i - 1, redirList);
			ptr = NULL;

			if(*token == '|') {
				pipeFlag = true;
				numFd += 2;
			}
			else {
				redirFlag = true;
				numFd+= 1;
			}
		}
		token = strtok(NULL, DELIMITERS);
	}
	tokens[i] = NULL;
	return tokens;
}

int main(int argc, char *argv[], char* envp[]) {
    char* input;
	char* curPath;
    bool builtinFlag = false;
	redirList = (SNode**)malloc(sizeof(SNode*));

    if(!isatty(STDIN_FILENO)) {
        // If your shell is reading from a piped file
        // Don't have readline write anything to that file.
        // Such as the prompt or "user input"
        if((rl_outstream = fopen("/dev/null", "w")) == NULL){
            perror("Failed trying to open DEVNULL");
            exit(EXIT_FAILURE);
        }
    }

	signal(SIGCHLD, sigchld_handler);
	signal(SIGINT, sigint_handler);
	signal(SIGTSTP, sigtstp_handler);

	sigemptyset(&sigMask);
	sigaddset(&sigMask, SIGCHLD);

    do {
		jump_active = 1;
		if(sigsetjmp(env, 1) == 42)
			write(STDOUT_FILENO, "\n", 1);

		size_t sizePwd = pathconf(".", _PC_PATH_MAX);
		if ((curPath = (char *)calloc(sizePwd, sizeof(char))) != NULL)
			getcwd(curPath, sizePwd);
		int promptSize = 12;
		char* homePtr;
		if((homePtr = strstr(curPath, HOMEBUF)) == NULL)
			promptSize += sizePwd;
		else
			promptSize += sizePwd - strlen(HOMEBUF) + 2;
		char *buf = (char*)calloc(promptSize, sizeof(char));
		if(homePtr == NULL)
			strncpy(buf, curPath, sizePwd);
		else {
			strncpy(buf, "~", 2);
			strcat(buf, homePtr + strlen(HOMEBUF));
		}
		strcat(buf, " :: ");
		strcat(buf, NETID);
		strcat(buf, " >> ");

		input = readline(buf);
		free(buf);
        // If EOF is read (aka ^D) readline returns NULL
        if(input == NULL) {
            continue;
        }

        //write(1, "\e[s", strlen("\e[s"));
        //write(1, "\e[20;10H", strlen("\e[20;10H"));
        //write(1, "SomeText", strlen("SomeText"));
        //write(1, "\e[u", strlen("\e[u"));

		char** tokens = splitLineIntoArgs(input);

		int* fd = (int*)calloc(numFd, sizeof(int));
		int idxFd = 0;
		int fdStdOut;
		int fdStdin;
		if(redirFlag || pipeFlag) {
			saveStdIO(&fdStdOut, &fdStdin);
			SNode* node = *redirList;
			while(node != NULL) {
				int idxRdr = peakSNode(node);
				if(strcmp(tokens[idxRdr],">") == 0 || strcmp(tokens[idxRdr],"<") == 0) {
					if(setRedirection(tokens, idxRdr, (fd + idxFd)) == -1)
						goto Final;
					idxFd++;
				}
				else {
					if(setPipe(tokens, idxRdr, (fd + idxFd)) == -1)
						goto Final;
					idxFd+=2;
				}
				node = node->next;
			}
			if(redirFlag)
				removeRdrFromTokens(tokens,redirList);
			if(pipeFlag)
				changePipeInTokens(tokens);
		}

		if(pipeFlag) {
			runPipeInput(tokens, curPath);
			goto Final;
		}
		
		if(tokens[0] != NULL && isBuiltIn(tokens, curPath)) {
			builtinFlag = true;
		}

		if(tokens[0] != NULL && builtinFlag == false) {
			int err = 0;
			if(strstr(tokens[0], "/") != NULL) {
				struct stat statBuf;
				if((err = stat(tokens[0], &statBuf)) != 0) {
					if(errno != EACCES || errno != ENOMEM || errno != EOVERFLOW)
						fprintf(stderr, EXEC_NOT_FOUND, tokens[0]);
					else
						fprintf(stderr, EXEC_ERROR,strerror(errno));
				}
				else {
					if(!(statBuf.st_mode & S_IXUSR)) {
						fprintf(stderr, EXEC_NOT_FOUND, tokens[0]);
						goto Final;
					}
				}
			}
			if(!err) {
				executable(tokens[0], tokens);
			}
		}

Final:
		builtinFlag = false;
		if(tokens != NULL) free(tokens);
		if(curPath != NULL) free(curPath);
		if(*redirList != NULL) removeSNode(redirList);
		if(redirFlag || pipeFlag) {
			restoreStdIO(&fdStdOut, &fdStdin);
			if(pipeFlag) clearPipe();
			dropFd(fd, numFd);
			numFd = 0;
		}
		redirFlag = false;
		pipeFlag = false;
		totalPgid = 1;
        // Readline mallocs the space for input. You must free it.
        rl_free(input);

    } while(1);

    debug("%s", "user entered 'exit'");
	free(redirList);

    return EXIT_SUCCESS;
}
