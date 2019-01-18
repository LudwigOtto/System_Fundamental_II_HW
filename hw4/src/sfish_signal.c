#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "sfish_signal.h"
#include "sfish_Ext.h"
#include <setjmp.h>
#include <errno.h>
#include <sys/wait.h>

volatile sig_atomic_t jump_active = 0;
volatile sig_atomic_t atPid;
sigset_t sigMask;
sigset_t sigPrev;
sigjmp_buf env;

void sigint_handler(int signo) {
	if (!jump_active) {
		return;
	}
	for(int i = 1; i < totalPgid; i++)
		kill(pgid[i], SIGINT);
	siglongjmp(env, 42);
}

void sigtstp_handler(int signo) {
	return;
}

void sigchld_handler(int signo) {
	int olderrno = errno;
	atPid = waitpid(-1, NULL, 0);
	kill(pgid[0], SIGCONT);
	errno = olderrno;
	return;
}
