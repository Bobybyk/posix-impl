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
static int initialiser_cond(pthread_cond_t *pcond){

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
 * Ouvre un smo et précise si celui-ci existait déjà
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
 * Initialise un rl_open_file nouvellement créé
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
