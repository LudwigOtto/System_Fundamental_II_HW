#ifndef SFISH_H
#define SFISH_H

/* Format Strings */
#define EXEC_NOT_FOUND "sfish: %s: command not found\n"
#define JOBS_LIST_ITEM "[%d] %s\n"
#define STRFTIME_RPRMT "%a %b %e, %I:%M%p"
#define BUILTIN_ERROR  "sfish builtin error: %s\n"
#define SYNTAX_ERROR   "sfish syntax error: %s\n"
#define EXEC_ERROR     "sfish exec error: %s\n"

#define TOKEN_SIZE 64
#define BUFFER_SIZE 256
#define DELIMITERS " \a\n\r\t"
#define REDIRECTORS "><|"
#define NETID "ykao"
#define HOMEBUF "/home/student"

#endif
