#ifndef SFISH_SIGNAL_H
#define SFISH_SIGNAL_H

#include "sfish.h"
#include <signal.h>
#include <setjmp.h>

typedef void (*sighandler_t)(int);
sighandler_t signal(int signum, sighandler_t handler);

void sigint_handler(int signo);

void sigtstp_handler(int signo);

void sigchld_handler(int signo);

extern volatile sig_atomic_t jump_active;
extern volatile sig_atomic_t atPid;
extern sigjmp_buf env;

extern sigset_t sigMask, sigPrev;

#endif
