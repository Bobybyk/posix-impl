#include "rl_lock_library.h"

static rl_all_files all_files;

/**
 * Initialise le mutex donnée en argument
 *
 * @param pmutex 	pointeur vers le mutex à initialiser
 * @return 		0 en cas de succès, valeur négative sinon
 */
static int initialiser_mutex(pthread_mutex_t *pmutex){

	pthread_mutexattr_t mutexattr;

	int ret;

	ret = pthread_mutexattr_init(&mutexattr);
	if(ret < 0) {
		return ret;
	}

	ret = pthread_mutexattr_setpshared(&mutexattr,	PTHREAD_PROCESS_SHARED);
	if(ret < 0) {
		return ret;
	}

	ret = pthread_mutex_init(pmutex, &mutexattr);

	return ret;
}

/**
 * Initialise la condition donnée en argument
 *
 * @param pcond 	pointeur vers la condition à initialiser
 * @return  		0 en cas de succès, valeur négative sinon
 */
static int initialiser_cond(pthread_cond_t *pcond) {

	pthread_condattr_t condattr;

	int ret;

	ret = pthread_condattr_init(&condattr) ;
	if(ret != 0)  {
		return ret;
	}

	ret = pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
	if(ret != 0) {
		return ret;
	}

	ret = pthread_cond_init(pcond, &condattr) ;

	return ret;
}


int rl_init_library() {

	all_files.nb_files = 0;

	return 0;
}

/**
 * Formate le nom du fichier dont le descripteur est passé en paramètre
 *
 * @param fd	descripteur du fichier$
 * @param dest	string dans lequel retourner le résultat
 * @return	0 en cas de succèes, -1 sinon
 */
static int smo_format_name(int fd, char *dest) {

	int ret;

	struct stat st;
	ret = fstat(fd, &st);
	if(ret < 0) {

		return -1;
	}

	// numéro de partition
	dev_t dev = st.st_dev;
	// numéro d'inode
	ino_t ino = st.st_ino;

	ret = sprintf(dest, "f_%ld_%ld", dev, ino);
	if(ret < 0) {

		return -1;
	}

	return 0;
}

/**
 * @brief Ouvre un smo et précise si celui-ci existait déjà
 *
 * @param name	Le nom a donner au nouveau fichier
 * @param new 	Le booléen dans lequel retourner si le fichier a été nouvellement créé
 * @return 	Le descripteur vers le fichier en cas de succès, -1 sinon
 */
static int smo_open(char *name, bool *new) {

	*new = true;

	int fd = shm_open(name, O_CREAT | O_RDWR | O_EXCL, 0644);
	if(fd >= 0) {

		if(ftruncate(fd, sizeof(rl_open_file)) < 0) {

			return -1;
		}
	} else if(fd < 0 && errno == EEXIST){

		fd = shm_open(name, O_RDWR, 0644);
		if(fd < 0) {

			return -1;	
		}
		*new = false;
	} else {

		return -1;
	}

	return fd;
}

/**
 * @brief Initialise un rl_open_file nouvellement créé
 *
 * @param file	Un pointeur vers la structure a initialiser
 * @return 	0 en cas de succès, -1 sinon
 */
static int rl_open_init_file(rl_open_file *file) {

	int ret;

	// première case non utilisée
	file->first = -2;
	
	for(int i = 0; i < NB_LOCKS; i++) {

		rl_lock *locks = file->lock_table;
		rl_lock *lock = &locks[i];

		lock->next_lock = -2;

		//file->lock_table[i].next_lock = -2;
	}		

	ret = initialiser_mutex(&file->mutex);
	if(ret < 0) {

		return -1;
	}

	ret = initialiser_cond(&file->cond);
	if(ret < 0) {

		return -1;
	}

	file->busy = false;

	return 0;
}

/**
 * @brief factorizes the duplicate code for dup and dup2
 * 
 * @param lfd 	The descriptor to duplicate 
 * @param newd 	The descriptor to use with duplication
 */
static void rl_dup_common_code(rl_descriptor lfd, int newd) {

	owner new_owner = {.proc = getpid(), .des = newd };

	rl_open_file *file = lfd.f;

	rl_lock *curr = &file->lock_table[file->first];

	//for all locks 
	while(curr->next_lock >= 0) {

		//for all owners
		for(size_t i=0; i < curr->nb_owners; i++) {

			owner ow = curr->lock_owners[i];

			//if the process is already owner
			if(ow.des == lfd.d && ow.proc == getpid()) {

				//add owner for new descriptor 
				curr->lock_owners[curr->nb_owners] = new_owner;
				curr->nb_owners++;
				break;
			}
		}

		curr = &file->lock_table[curr->next_lock];
	}
}

/**
 * @brief Duplicates a descriptor
 * 
 * @param lfd The descriptor to duplicate
 *
 * @return The new rl_descriptor 
 */
rl_descriptor rl_dup(rl_descriptor lfd) {

	rl_descriptor new_rl_descriptor = { .d = -1, .f = NULL};
	
	int newd = dup(lfd.d);
	if(newd < 0) {
		
		return new_rl_descriptor;
	}

	rl_dup_common_code(lfd, newd);

	new_rl_descriptor.d = newd; 
	new_rl_descriptor.f = lfd.f;

	return new_rl_descriptor;
}

/**
 * @brief Duplicates a descriptor
 * 
 * @param lfd 	The descriptor to duplicate
 * @param newd 	The descriptor to use
 *
 * @return 	The new rl_descriptor 
 */
rl_descriptor rl_dup2(rl_descriptor lfd, int newd) {

	rl_descriptor new_rl_descriptor = { .d = -1, .f = NULL};
	
	int ret = dup2(lfd.d, newd);
	if(ret < 0) {
		
		return new_rl_descriptor;
	}

	rl_dup_common_code(lfd, newd);

	new_rl_descriptor.d = newd; 
	new_rl_descriptor.f = lfd.f;

	return new_rl_descriptor;
}

pid_t rl_fork() {

	pid_t pid = fork();

	if(pid < 0) {

		return -1;
	} else if(pid > 0) {
		
		return pid;
	}
	
	pid_t parent = getppid();

	//for all files
	for(int i=0; i<all_files.nb_files; i++) {
		
		rl_open_file *file = all_files.tab_open_files[i];

		rl_lock *curr = &file->lock_table[file->first];

		//for all locks 
		while(curr->next_lock >= 0) {

			//for all owners
			for(size_t i=0; i < curr->nb_owners; i++) {

				owner ow = curr->lock_owners[i];

				//if the process is already owner
				if(ow.proc == parent) {
					curr->lock_owners[curr->nb_owners] = (owner) { .proc = getpid(), .des = ow.des};
					curr->nb_owners++;
				}
			}

			curr = &file->lock_table[curr->next_lock];
		}
	}
	
	return 0;
}

rl_descriptor rl_open(const char *path, int oflag, ...) {

	int ret;

	// initialisation avec valeurs par défaut "erronnées"
	rl_descriptor desc = {
		.d = -1,
		.f = NULL
	};

	va_list ap;

	// arguments dans ap, oflag dernier agument présent sûr
	va_start(ap, oflag);

	mode_t perms = va_arg(ap, mode_t);

	va_end(ap);

	int fd = open(path, oflag, perms);
	if(fd < 0) {

		return desc;
	}

	char smo_name[100];
	bool smo_created;

	//  section 5.3 du sujet, shared memory object
	ret = smo_format_name(fd, smo_name);
	if(ret < 0) {

		return desc;
	}

	int smo_fd = smo_open(smo_name, &smo_created);
	if(smo_fd < 0) {

		return desc;
	}

	rl_open_file *file = mmap(NULL, sizeof(rl_open_file), PROT_READ | PROT_WRITE, MAP_SHARED, smo_fd, 0);

	if (file == MAP_FAILED) {

		return desc;
	}

	ret = close(smo_fd);

	if (ret < 0) {

		return desc;
	}

	if(smo_created) {

		ret = rl_open_init_file(file);
		if(ret < 0) {

			return desc;
		}
	}

	// initialisation avec bonnes valeurs
	desc.d = fd;
	desc.f = file;

	return desc;
}
