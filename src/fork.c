#include <stdlib.h>
#include <stdio.h>

#include "rl_lock_library.h"

int main(void) {

	rl_descriptor desc = rl_open("test", O_RDONLY | O_CREAT, 0644);
	if(desc.d < 0 || desc.f == NULL) {
		perror("rl_open");
		return EXIT_FAILURE;
	}

	struct flock lock = {
		.l_len = 100,
		.l_start = 0,
		.l_whence = SEEK_SET,
		.l_type = F_RDLCK
	};

	int ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		return EXIT_FAILURE;
	}

	pid_t pid = rl_fork();
	if(pid == -1) {
		perror("rl_fork");
	} else if(pid == 0) {	//enfant

		puts("enfant");
		rl_debug();

		lock.l_start = 300;
		lock.l_len = 50;
		ret = rl_fcntl(desc, F_SETLK, &lock);
		if(ret < 0) {
			perror("rl_fcntl");
			return EXIT_FAILURE;
		}
		puts("verrou posé");
		rl_debug();
	} else {		//parent

		sleep(5);

		puts("parent");
		rl_debug();

		wait(NULL);
	}

	rl_close(desc);

	return EXIT_SUCCESS;
}
