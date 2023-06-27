#include <stdlib.h>
#include <stdio.h>

#include "rl_lock_library.h"

int main(void) {

	/* rl_descriptor desc = rl_open("test", O_RDONLY | O_CREAT, 0644);
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
	puts("verrou posé");
	rl_debug();

	lock.l_start = 50;
	lock.l_len = 150;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		return EXIT_FAILURE;
	}
	puts("verrou posé");
	rl_debug();

	lock.l_start = 220;
	lock.l_len = 50;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		return EXIT_FAILURE;
	}
	puts("verrou posé");
	rl_debug();

	lock.l_start = 199;
	lock.l_len = 22;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		return EXIT_FAILURE;
	}
	puts("verrou posé");
	rl_debug();

	lock.l_start = 40;
	lock.l_len = 5;
	lock.l_type = F_UNLCK;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		return EXIT_FAILURE;
	}
	puts("verrou posé");
	rl_debug();
	
	rl_close(desc); */

	rl_descriptor desc = rl_open("test", O_RDONLY | O_CREAT, 0644);
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

	int ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		return EXIT_FAILURE;
	}
	puts("verrou posé");
	rl_debug();

	lock.l_start = 150;
	lock.l_len = 100;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		return EXIT_FAILURE;
	}
	puts("verrou posé");
	rl_debug();

	// READ

	lock.l_start = 20;
	lock.l_len = 20;
	lock.l_type = F_RDLCK;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0 && errno == EAGAIN) {
		perror("rl_fcntl");
	}
	puts("verrou posé");
	rl_debug();

	lock.l_start = 250;
	lock.l_len = 50;
	
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0) {
		perror("rl_fcntl");
		return EXIT_FAILURE;
	}
	puts("verrou posé");
	rl_debug();

	// WRITE

	lock.l_start = 260;
	lock.l_len = 20;
	lock.l_type = F_WRLCK;
	ret = rl_fcntl(desc, F_SETLK, &lock);
	if(ret < 0 && errno == EAGAIN) {
		perror("rl_fcntl");
	}
	puts("verrou posé");
	rl_debug();

	rl_close(desc);

	return EXIT_SUCCESS;
}
