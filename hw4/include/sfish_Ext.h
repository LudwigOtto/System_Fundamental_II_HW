#ifndef SFISH_EXT_H
#define SFISH_EXT_H

#include "sfish.h"
#include <signal.h>

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);

struct SfishList {
	int val;
	struct SfishList *next;
};
typedef struct SfishList SNode;

SNode* createSNode(int val);

SNode* addSNode(int val, SNode** head);

int peakSNode(SNode* node);

void removeSNode(SNode** head);

int setRedirection(char** tokens, int idxRdr, int* fd);

int setPipe(char** tokens, int idxRdr, int* fd);

void removeRdrFromTokens(char** tokens, SNode** redirList);

void changePipeInTokens(char** tokens);

void saveStdIO(int* savedOut, int* savedIn);

void restoreStdIO(int* savedOut, int* savedIn);

void dropFd(int* fd, int numFd);

void clearPipe();

int builtInCd(char** tokens, char* curPath);

int builtInPwd(char* curPath);

int builtInHelp();

int isBuiltIn(char** tokens, char* curPath);

void executable(char* path, char** argv);

void runPipeInput(char** tokens, char* curPath);

extern char oldPWD[BUFFER_SIZE];

#define MAX_NUM_PGID 256

extern int pgid[MAX_NUM_PGID];

extern volatile int totalPgid;
#endif
