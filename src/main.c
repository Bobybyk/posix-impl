#include <stdlib.h>
#include <stdio.h>

#include "rl_lock_library.h"

int main(void) {

	rl_descriptor desc = rl_open("taest", O_RDONLY | O_CREAT, 0644);
	if(desc.d < 0 || desc.f == NULL) {
		perror("rl_open");
		return EXIT_FAILURE;
	}

	printf("%d\t%p\n", desc.d, desc.f);

	return EXIT_SUCCESS;
}
