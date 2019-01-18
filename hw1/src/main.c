#include <stdlib.h>

#include "hw1.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

int main(int argc, char **argv)
{
    unsigned short mode;

    mode = validargs(argc, argv);

    //debug("Mode: 0x%X", mode);

	if(mode == HELP_CODE) {
        USAGE(*argv, EXIT_SUCCESS);
	}
	else if( mode == FAULT_CODE) {
		USAGE(*argv, EXIT_FAILURE);
	}
	else {
		if(activateCrypto(mode))
			return EXIT_FAILURE;
	}

    return EXIT_SUCCESS;
}

/*
 * Just a reminder: All non-main functions should
 * be in another file not named main.c
 */