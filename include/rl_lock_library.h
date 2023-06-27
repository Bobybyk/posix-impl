#ifndef RL_LOCK_LIBRARY_H
#define RL_LOCK_LIBRARY_H

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "panic.h"

#define NB_FILES 256
#define NB_LOCKS 20
#define NB_OWNERS 5

/*	STRUCTS 	*/
/*======================*/

typedef struct{
	pid_t proc; 	/* pid du processus */
	int des;	/* descripteur de fichier */
} owner;

typedef struct{
	int next_lock;
	off_t starting_offset;
	off_t len;
	short type; 			/* F_RDLCK F_WRLCK */
	size_t nb_owners;
	owner lock_owners[NB_OWNERS];
} rl_lock;

typedef struct{
	int first;
	int open_files_count;
	rl_lock lock_table[NB_LOCKS];
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool busy;
} rl_open_file;

typedef struct{
	int d;
	rl_open_file *f;
} rl_descriptor;

typedef struct {
	int nb_files;
	rl_open_file *tab_open_files[NB_FILES];
} rl_all_files;

/*	FUNCTIONS 	*/
/*======================*/

int rl_init_library();

rl_descriptor rl_open(const char *path, int oflag, ...);

int rl_close( rl_descriptor lfd);

int rl_fcntl(rl_descriptor lfd, int cmd, struct flock *lck);

rl_descriptor rl_dup(rl_descriptor lfd);

rl_descriptor rl_dup2(rl_descriptor lfd, int newd);

pid_t rl_fork();

void rl_debug();

#endif 
