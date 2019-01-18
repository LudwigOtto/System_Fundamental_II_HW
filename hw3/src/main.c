#include <stdio.h>
#include "sfmm.h"

int main(int argc, char const *argv[]) {

    sf_mem_init();

    double* ptr = sf_malloc(sizeof(double));

    *ptr = 320320320e-320;

    printf("ptr1: %e\n", *ptr);

    double* ptr2 = sf_malloc(100*sizeof(double));
    *ptr2 = 100;
    printf("ptr2: %e\n", *ptr2);

    sf_free(ptr);
	
    double* ptr3 = sf_realloc(ptr2, 10*sizeof(double));
    printf("ptr3: %e\n", *ptr3);

	sf_free(ptr3);

    sf_mem_fini();

    return EXIT_SUCCESS;
}
