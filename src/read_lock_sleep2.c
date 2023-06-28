#include <stdlib.h>
#include <stdio.h>

#include "rl_lock_library.h"

int main(void) {

	rl_descriptor desc = rl_open("fichier_verrous", O_RDONLY | O_CREAT, 0644);
	  if(desc.d < 0 || desc.f == NULL) {
		perror("rl_open");
		return EXIT_FAILURE;
	}
	/*
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

    printf("Appuyez sur la touche ENTER pour déverrouiller...\n");
    while (getchar() != '\n'){}
    printf("Déverrouillé !\n");
	
*/
	rl_debug();
	rl_close(desc); 

	return EXIT_SUCCESS;
}
