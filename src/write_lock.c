#include <stdlib.h>
#include <stdio.h>

#include "rl_lock_library.h"

int main(void) {

  	rl_init_library();

    rl_descriptor desc = rl_open("fichier_verrous", O_RDONLY | O_CREAT, 0644);
	if(desc.d < 0 || desc.f == NULL) {
		perror("rl_open");
		return EXIT_FAILURE;
	}

	struct flock lock = {
		.l_len = 100,
		.l_start = 0,
		.l_whence = SEEK_SET,
		.l_type = F_WRLCK
	};

	// WRITE
	
	printf("=============\n");

	int ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		printf("echec posée verrou écriture\n");
		return EXIT_FAILURE;
	} else {
		puts("verrou écriture posé");
	}
	rl_debug();

	printf("=============\n");

	lock.l_start = 150;
	lock.l_len = 100;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		printf("echec posée verrou écriture\n");
		return EXIT_FAILURE;
	} else {
		puts("verrou écriture posé");
	}
	rl_debug();

	printf("=============\n");

	// READ

	lock.l_start = 20;
	lock.l_len = 20;
	lock.l_type = F_RDLCK;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0 && errno == EAGAIN) {
		perror("rl_fcntl");
		printf("echec posée verrou lecture\n");
	} else {
		puts("verrou lecture posé");
	}
	rl_debug();

	printf("=============\n");

	lock.l_start = 250;
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

	// WRITE

	lock.l_start = 249;
	lock.l_len = 60;
	lock.l_type = F_WRLCK;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0 && errno == EAGAIN) {
		perror("rl_fcntl");
		printf("echec posée verrou écriture\n");
	} else {
		puts("verrou écriture posé");
	}
	rl_debug();

	printf("=============\n");

	rl_close(desc);

	return EXIT_SUCCESS;
}
