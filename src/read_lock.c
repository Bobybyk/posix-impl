#include <stdlib.h>
#include <stdio.h>

#include "rl_lock_library.h"

int main(void) {

	rl_descriptor desc = rl_open("fichier_verrous", O_RDONLY | O_CREAT, 0644);
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

	printf("=============\n");

	int ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		printf("echec posée verrou lecture\n");
		return EXIT_FAILURE;
	} else {
		puts("verrou lecture posé");
	}
	rl_debug();

	printf("=============\n");

	lock.l_start = 50;
	lock.l_len = 150;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		printf("echec posée verrou lecture\n");
		return EXIT_FAILURE;
	} else {
		puts("verrou lecture posé");
	}
	rl_debug();
	
	printf("=============\n");

	lock.l_start = 220;
	lock.l_len = 50;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		printf("echec posée verrou lecture\n");
		return EXIT_FAILURE;
	} else {
		puts("verrou lecture posé");
	}
	rl_debug();

	printf("=============\n");

	lock.l_start = 199;
	lock.l_len = 22;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		printf("echec posée verrou lecture\n");
		return EXIT_FAILURE;
	} else {
		puts("verrou lecture posé");
	}
	rl_debug(); 

	printf("=============\n");

	lock.l_start = 40;
	lock.l_len = 5;
	lock.l_type = F_UNLCK;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		printf("echec suppression verrou lecture\n");
		return EXIT_FAILURE;
	} else {
		puts("verrou lecture supprimé");
	}
	rl_debug();

	printf("=============\n");

	rl_close(desc);

	return EXIT_SUCCESS;
}
