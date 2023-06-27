#include "rl_lock_library.h"

static rl_all_files all_files;

/**
 * Initialise le mutex donnée en argument
 *
 * @param pmutex 	pointeur vers le mutex à initialiser
 * @return 		0 en cas de succès, valeur négative sinon
 */
static int init_mutex(pthread_mutex_t *pmutex) {

	pthread_mutexattr_t mutexattr;

	int ret;

	ret = pthread_mutexattr_init(&mutexattr);
	if (ret < 0) {
		return ret;
	}

	ret = pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
	if (ret < 0) {
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
static int init_cond(pthread_cond_t *pcond) {

	pthread_condattr_t condattr;

	int ret;

	ret = pthread_condattr_init(&condattr);
	if (ret != 0) {
		return ret;
	}

	ret = pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
	if (ret != 0) {
		return ret;
	}

	ret = pthread_cond_init(pcond, &condattr);

	return ret;
}

int rl_init_library() {

	all_files.nb_files = 0;

	return 0;
}

/**
 * Formate le nom du fichier dont le descripteur est passé en paramètre
 *
 * @param fd	descripteur du fichier
 * @param dest	string dans lequel retourner le résultat
 * @return	0 en cas de succèes, -1 sinon
 */
static int smo_format_name(int fd, char *dest) {

	int ret;

	struct stat st;
	ret = fstat(fd, &st);
	if (ret < 0) {

		return -1;
	}

	// numéro de partition
	dev_t dev = st.st_dev;
	// numéro d'inode
	ino_t ino = st.st_ino;

	ret = sprintf(dest, "/f_%ld_%ld", dev, ino);
	if (ret < 0) {

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
	if (fd >= 0) {

		if (ftruncate(fd, sizeof(rl_open_file)) < 0) {

			return -1;
		}
	}
	else if (fd < 0 && errno == EEXIST) {

		fd = shm_open(name, O_RDWR, 0644);
		if (fd < 0) {

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

	for (int i = 0; i < NB_LOCKS; i++) {

		rl_lock *locks = file->lock_table;
		rl_lock *lock = &locks[i];

		lock->next_lock = -2;

	}

	ret = init_mutex(&file->mutex);
	if (ret < 0) {
		return -1;
	}

	ret = init_cond(&file->cond);
	if (ret < 0) {
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

	owner new_owner = {.proc = getpid(), .des = newd};

	rl_open_file *file = lfd.f;

	rl_lock *curr = &file->lock_table[file->first];

	// for all locks
	while (curr->next_lock >= 0) {

		// for all owners
		for (size_t i = 0; i < curr->nb_owners; i++) {

			owner ow = curr->lock_owners[i];

			// if the process is already owner
			if (ow.des == lfd.d && ow.proc == getpid()) {

				// add owner for new descriptor
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

	rl_descriptor new_rl_descriptor = {.d = -1, .f = NULL};

	int newd = dup(lfd.d);
	if (newd < 0) {
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

	rl_descriptor new_rl_descriptor = {.d = -1, .f = NULL};

	int ret = dup2(lfd.d, newd);
	if (ret < 0) {
		return new_rl_descriptor;
	}

	rl_dup_common_code(lfd, newd);

	new_rl_descriptor.d = newd;
	new_rl_descriptor.f = lfd.f;

	return new_rl_descriptor;
}

pid_t rl_fork() {

	pid_t pid = fork();

	if (pid < 0) {
		return -1;
	}
	else if (pid > 0) {
		return pid;
	}

	pid_t parent = getppid();

	// for all files
	for (int i = 0; i < all_files.nb_files; i++) {

		rl_open_file *file = all_files.tab_open_files[i];
		pthread_mutex_lock(&file->mutex);
		while(file->busy) {
			pthread_cond_wait(&file->cond, &file->mutex);
		}
		file->busy = true;

		rl_lock *curr = &file->lock_table[file->first];

		// for all locks
		do {

			// for all owners
			for (size_t i = 0; i < curr->nb_owners; i++) {

				owner ow = curr->lock_owners[i];

				// if the process is already owner
				if (ow.proc == parent) {
					curr->lock_owners[curr->nb_owners] = (owner){.proc = getpid(), .des = ow.des};
					curr->nb_owners++;
				}
			}

			curr = &file->lock_table[curr->next_lock];
		} while (curr->next_lock >= 0);

		file->busy = false;
		pthread_mutex_unlock(&file->mutex);
		pthread_cond_signal(&file->cond);
	}

	return 0;
}

int rl_close(rl_descriptor lfd) {
    rl_open_file *file = lfd.f;
    owner lfd_owner = {.proc = getpid(), .des = lfd.d};

    pthread_mutex_lock(&file->mutex);
    while (file->busy) {
        pthread_cond_wait(&file->cond, &file->mutex);
    }
    file->busy = true;

    // Parcourt le tableau des verrous et supprime les propriétaires
    for (int i = 0; i < NB_LOCKS; i++) {
        rl_lock *lock = &file->lock_table[i];

        // Vérifie chaque propriétaire
        for (size_t j = 0; j < lock->nb_owners; j++) {
            // Si le propriétaire correspond à lfd_owner, on le supprime
            owner lock_owner = lock->lock_owners[j];
            if (lock_owner.proc == lfd_owner.proc && lock_owner.des == lfd_owner.des) {
				// on supprime le propriétaire et on supprime le verrou s'il n'y a plus qu'un propriétaire dans rl_open_file
				lock->lock_owners[j] = lock->lock_owners[lock->nb_owners - 1];
				lock->nb_owners--;
				if (lock->nb_owners == 0) {
					// on supprime le verrou
					if (lock->next_lock >= 0) {
						// on supprime le verrou de la liste
						rl_lock *next_lock = &file->lock_table[lock->next_lock];
						lock->next_lock = next_lock->next_lock;
						next_lock->next_lock = -2;
						file->open_files_count--;
					}
					else {
						// on supprime le verrou de la liste
						file->first = -2;
					}
				}
                break;
            }
        }
    }

	// on supprime le smo si plus aucun processus n'utilise le fichier
	if (file->open_files_count <= 0) {
		char name[256];
		smo_format_name(lfd.d, name);
		printf("shm_unlink %s\n", name);
		shm_unlink(name);
	}

    file->busy = false;
    pthread_mutex_unlock(&file->mutex);
    pthread_cond_signal(&file->cond);

    // Libère la mémoire
    if (munmap(lfd.f, sizeof(rl_open_file)) == -1) {
        perror("munmap");
        return -1;
    }

    // Ferme le descripteur et retourne le résultat
    return close(lfd.d);
}

rl_descriptor rl_open(const char *path, int oflag, ...) {

	int ret;

	// initialisation avec valeurs par défaut "erronnées"
	rl_descriptor desc = {
		.d = -1,
		.f = NULL};

	va_list ap;

	// arguments dans ap, oflag dernier agument présent sûr
	va_start(ap, oflag);

	mode_t perms = va_arg(ap, mode_t);

	va_end(ap);

	int fd = open(path, oflag, perms);
	if (fd < 0) {
		return desc;
	}

	char smo_name[100];
	bool smo_created;

	//  section 5.3 du sujet, shared memory object
	ret = smo_format_name(fd, smo_name);
	if (ret < 0) {
		return desc;
	}

	int smo_fd = smo_open(smo_name, &smo_created);
	if (smo_fd < 0) {
		return desc;
	}

	rl_open_file *file = mmap(NULL, sizeof(rl_open_file), PROT_READ | PROT_WRITE, MAP_SHARED, smo_fd, 0);

	if (file == MAP_FAILED) {
		return desc;
	}

	file->open_files_count++;

	ret = close(smo_fd);

	if (ret < 0) {
		return desc;
	}

	if (smo_created) {

		ret = rl_open_init_file(file);
		if (ret < 0) {
			return desc;
		}
	}

	for (int i = 0; i < NB_LOCKS; i++) {
		file->lock_table[i].next_lock = -2;
	}

	// initialisation avec bonnes valeurs
	desc.d = fd;
	desc.f = file;

	all_files.tab_open_files[all_files.nb_files] = file;
	all_files.nb_files++;

	return desc;
		pthread_mutex_unlock(&file->mutex);
		pthread_cond_signal(&file->cond);
}

/**
 * @brief ajoute un verrou dans la liste de verrous
 * @param lfd descripteur de fichier
 * @param lck verrou à ajouter
 * @return 0 si succès, -1 sinon
 */
int add_lock(rl_descriptor *lfd, rl_lock * lck) {
	int ptr = lfd->f->first;
	if (ptr == -2) {
		lck->next_lock = -1;
		lfd->f->lock_table[0] = *lck;
		return 0;
	}
	// ajout du lock dans une case libre
	int i;
	for (i = 0; i < NB_LOCKS; i++) {
		if (lfd->f->lock_table[i].next_lock == -2)break;
	}
	int prev = 0;
	// parcours de la liste de lock
	while (ptr != -2 && ptr != -1) {
		prev = ptr;
		ptr = lfd->f->lock_table[ptr].next_lock;
	}
	lck->next_lock = -1;
	if(prev != i) lfd->f->lock_table[prev].next_lock = i;
	lfd->f->lock_table[i] = *lck;
	return i;
}

/**
 *   Cette function supprime un lock
 *   @param lock: Le lock à supprimer
 *   @param rl_descriptor: Le pointeur vers le descripteur auquel il faut supprimer le lock
 */
static void delete_lock(rl_descriptor *des, int lock) {
    if (des->f->first != lock) {
        int i = des->f->first;
        while (i != -1) {
            if (des->f->lock_table[i].next_lock == lock) {
                des->f->lock_table[i].next_lock = des->f->lock_table[lock].next_lock;
                break;
            }
            i = des->f->lock_table[i].next_lock;
        }
    } else {
        if (des->f->lock_table[lock].next_lock != -1) {           
            des->f->first = des->f->lock_table[lock].next_lock;
        } else {// SI PAS DE NEXT LOCK
            des->f->first = -2;
        }
    }
    des->f->lock_table[lock].next_lock = -2;
}

/**
 * @brief transforme un rl_lock en rl_lock avec les nouvelles coordonnées
 * 	[##### 0 - 100 ######] -> [### 10 - 80 ###]
 * @param start: Nouveau début du lock
 * @param end: Nouveau fin du lock
 * @param lfd: Descripteur de fichier
 * @param copy: Le lock à copier
 */
void change_lock_position(int start, int end, rl_descriptor *lfd, rl_lock * copy) {
	rl_lock new_r_lock = {
		.next_lock = copy->next_lock, 
		.type = copy->type, 
		.nb_owners = copy->nb_owners, 
		.starting_offset = start, 
		.len = end - start
		};
	for (size_t i = 0; i < copy->nb_owners; i++) {
		owner copyowner = {
			.des = copy->lock_owners[i].des,
			.proc = copy->lock_owners[i].proc};
		new_r_lock.lock_owners[i] = copyowner;
	}
	add_lock(lfd, &new_r_lock);
}

/**
 * @brief supprime un verrou dans la liste de verrous
 * @param lfd descripteur de fichier
 * @param lck verrou à supprimer
 * @return 0 si succès, -1 sinon
 */
int rl_fcntl_unlock_all(rl_descriptor *lfd, struct flock *lck) {
	int lock_start = lck->l_start;
	int end_lock = lck->l_start + lck->l_len;

	int i = lfd->f->first;
	while (i != -2 && i != -1) {
		rl_lock *current_lock = &lfd->f->lock_table[i];		
		int current_lock_start = current_lock->starting_offset;
		int current_lock_end = current_lock->starting_offset + current_lock->len;

		if (lock_start > current_lock_start && current_lock_end > end_lock) {
			// Cas particulier: si le unlock verrou est en plein milieu d'un verrou
			//  [### VEROU ####]
			//         [######### SUPRESSION VEROU ############]
			change_lock_position(current_lock_start, lock_start, lfd, current_lock);
		}
		// Cas classique: current_lock_start > lock_start
		//           [#### VERROU ####] [### VERROU 2 ####] etc..
		//  [############## SUPPRESSION VERROU ###########################]
		delete_lock(lfd, i);
		if (current_lock->next_lock == -1) break;
		i = lfd->f->lock_table[i].next_lock;
	}
	return EXIT_SUCCESS;
}

/**
 * @brief supprime un verrou dans la liste de verrous
 * @param lfd descripteur de fichier
 * @param lck verrou à supprimer
 * @return 0 si succès, -1 sinon
 */
int rl_fcntl_unlock_pos(rl_descriptor *lfd, struct flock *lck) {
	int lock_start = lck->l_start;
	int end_lock = lck->l_start + lck->l_len;

	int i = lfd->f->first;
	while (i != -2 && i != -1) {
			rl_lock * current_lock = &lfd->f->lock_table[i];

			int current_lock_start = current_lock->starting_offset;
			int current_lock_end = current_lock->starting_offset + current_lock->len;

			if (current_lock_start == lock_start && current_lock_end == end_lock) {
				delete_lock(lfd, i);
			} 
			else if (lock_start >= current_lock_start && lock_start < current_lock_end && current_lock_end < end_lock) {
				// [#### VEROU ####]
				//            [######### SUPRESSION VEROU ############]
				change_lock_position(current_lock_start, lock_start, lfd, current_lock);
				delete_lock(lfd, i);
			}
			else if (lock_start <= current_lock_start && current_lock_end >= end_lock && end_lock > current_lock_start) {
				// 						  [### VEROU ####]
				//        [## SUPRESSION VEROU ##]
				change_lock_position(end_lock, current_lock_end, lfd, current_lock);
				delete_lock(lfd, i);
			}
			else if (lock_start >= current_lock_start && current_lock_end >= end_lock) {
				// 		[###### VEROU #######]
				//             [SVEROU]
				change_lock_position(current_lock_start, lock_start, lfd, current_lock);
				change_lock_position(end_lock, current_lock_end, lfd, current_lock);
				delete_lock(lfd, i);
			}
			
			//           [#### VERROU ####]
			//       [######deleteLock########]
		if (current_lock->next_lock == -1) break;
		i = lfd->f->lock_table[i].next_lock;
	}
	return EXIT_SUCCESS;
}

/**
 * @brief Permet de retirer un verrou
 * @param lfd descripteur de fichier
 * @param lck verrou à retirer
 * @return 0 si succès, -1 sinon
*/
int rl_fcntl_unlock(rl_descriptor *lfd, struct flock *lck) {

	if (lck->l_len == 0) {
		return rl_fcntl_unlock_all(lfd, lck); // Cas retirer tous les verrous
	} else {
		return rl_fcntl_unlock_pos(lfd, lck); // Cas particulier
	}
	return EXIT_SUCCESS;
}

/**
 * @brief Permet de s'assurer que deux verrous ne se chevauchent pas
 * @return true si les deux verrous se chevauchent, false sinon
 * @param lck1 le premier verrou
 * @param lck2 le deuxième verrou
*/
static bool lock_overlap(rl_lock lck1, rl_lock lck2) {
	return (lck1.starting_offset < lck2.starting_offset && (lck1.starting_offset + lck1.len > lck2.starting_offset)) 
	|| (lck2.starting_offset < lck1.starting_offset && (lck2.starting_offset + lck2.len > lck1.starting_offset)) 
	|| (lck1.starting_offset == lck2.starting_offset);
}

/**
 * @brief Permet de fusionner deux verrous en s'assurant que les contraintes sont respectées
 * @param lock1 le premier verrou
 * @param lock2 le deuxième verrou
*/
static void merge_locks_aux(rl_lock *lock1, rl_lock *lock2) {
	
	size_t start_lock_1 = lock1->starting_offset;
	size_t end_lock_1 = lock1->starting_offset + lock1->len;
	
	size_t start_lock_2 = lock2->starting_offset;
	size_t end_lock_2 = lock2->starting_offset + lock2->len;

	// Cas classique fusion
	// [LOCK 1]
	//       [ LOCK 2]
	//
	if(end_lock_1 >= start_lock_2 && end_lock_1 <= end_lock_2) {
		if(lock1->nb_owners == 1 && lock2->nb_owners == 1) {
			lock1->len += (lock2->len) - (end_lock_1 - start_lock_2);
			lock1->next_lock = lock2->next_lock;
			lock2->next_lock = -2;
		}
	}
	else if(start_lock_1 < end_lock_2 && end_lock_2 < end_lock_1) {
	// Cas classique fusion
	// 				[LOCK 1]
	//       [ LOCK 2]
	//
		if(lock1->nb_owners == 1 && lock2->nb_owners == 1) {
			lock1->starting_offset = lock2->starting_offset;
			lock1->len += (lock2->len) - (end_lock_2 - start_lock_1);
			lock1->next_lock = lock2->next_lock;
			lock2->next_lock = -2;
		}
	}
}

/**
 * @brief permet de fusionner les verrous qui se chevauchent et dont le type est compatible
 * @param lfd le descripteur de fichier
 * @return 0 si tout s'est bien passé, -1 sinon
*/
static int merge_locks(rl_descriptor *lfd) {

	rl_open_file *file = lfd->f;

	rl_lock *lock1 = &file->lock_table[file->first];

	if (file->first == -2) {
		return 0;
	}

	do {
		rl_lock *lock2 = &file->lock_table[lock1->next_lock];
			
		if(lock_overlap(*lock1, *lock2)) {
			//puts("overlap");
			merge_locks_aux(lock1, lock2);
		}

		lock1 = &file->lock_table[lock1->next_lock];
	} while(lock1->next_lock >= 0);

	return 0;
}

/**
 * @brief Permet de poser un verrou en écriture en s'assurant que les contraintes sont respectées
 * On parcourt tous les blocs, pour chaque bloc rencontré, si les blocs se chevauchent et que le bloc chevauché est 
 * un verrou en écriture et qu'il n'y a qu'un seul propriétaire, on ajoute le verrou,
 * sinon si le bloc chevauché est un verrou en lecture et qu'il n'y a qu'un seul propriétaire, 
 * on supprime le verrou en lecture et on ajoute le verrou en écriture, sinon on rejette la demande
 * @param lfd le descripteur de fichier
 * @param lck le verrou à poser
 * @return 0 si tout s'est bien passé, -1 sinon
*/
static int rl_fcntl_wlock(rl_descriptor *lfd, struct flock *lck) {
    rl_lock new_lock = {
        .next_lock = -1,
        .starting_offset = lck->l_start,
        .len = lck->l_len,
        .type = lck->l_type,
        .nb_owners = 1
    };

    new_lock.lock_owners[0] = (owner) {
        .proc = getpid(),
        .des = lfd->d
    };

    rl_open_file *file = lfd->f;

    pthread_mutex_lock(&file->mutex);
    while (file->busy) {
        pthread_cond_wait(&file->cond, &file->mutex);
    }
    file->busy = true;

    if (file->first == -2) {
        file->lock_table[0] = new_lock;
        file->first = 0;
		
		file->busy = false;
		pthread_mutex_unlock(&file->mutex);
		pthread_cond_signal(&file->cond);
        return 0;
    }

    int prev_lock_index = -1;
    int curr_lock_index = file->first;

    while (curr_lock_index >= 0) {
        rl_lock *curr_lock = &file->lock_table[curr_lock_index];

        if (curr_lock->starting_offset > new_lock.starting_offset) {
            break;
        }

        if (curr_lock->starting_offset + curr_lock->len > new_lock.starting_offset) {
            if (curr_lock->type == F_WRLCK && curr_lock->nb_owners == 1) {
                // Remove existing write lock and add the new write lock
                if (prev_lock_index >= 0) {
                    file->lock_table[prev_lock_index].next_lock = curr_lock->next_lock;
                } else {
                    file->first = curr_lock->next_lock;
                }

                new_lock.next_lock = curr_lock->next_lock;
                file->lock_table[curr_lock_index] = new_lock;

				file->busy = false;
                pthread_mutex_unlock(&file->mutex);
				pthread_cond_signal(&file->cond);
                return 0;
            } else if (curr_lock->type == F_RDLCK && curr_lock->nb_owners == 1) {
                // Remove existing read lock and add the new write lock
                if (prev_lock_index >= 0) {
                    file->lock_table[prev_lock_index].next_lock = curr_lock->next_lock;
                } else {
                    file->first = curr_lock->next_lock;
                }

                new_lock.next_lock = curr_lock->next_lock;
                file->lock_table[curr_lock_index] = new_lock;

				file->busy = false;
                pthread_mutex_unlock(&file->mutex);
				pthread_cond_signal(&file->cond);
                return 0;
            } else {
				file->busy = false;
                pthread_mutex_unlock(&file->mutex);
				pthread_cond_signal(&file->cond);
                return -1;  // Reject the request if the lock cannot be acquired
            }
        }

        prev_lock_index = curr_lock_index;
        curr_lock_index = curr_lock->next_lock;
    }

    // Add the new lock at the end of the table
    int new_lock_index = 0;
    while (file->lock_table[new_lock_index].next_lock >= 0) {
        new_lock_index = file->lock_table[new_lock_index].next_lock;
    }
    file->lock_table[new_lock_index].next_lock = new_lock_index + 1;
    new_lock.next_lock = -1;
    file->lock_table[new_lock_index + 1] = new_lock;

	file->busy = false;
	pthread_mutex_unlock(&file->mutex);
	pthread_cond_signal(&file->cond);
    return 0;
}


/**
 * @brief Permet de poser un verrou en lecture en s'assurant que les contraintes sont respectées
 * On parcourt tous les blocs, pour chaque bloc rencontré, si les blocs se chevauchent et que le bloc chevauché est
 * un verrou en écriture, on rejette la demande, sinon si le bloc chevauché est un verrou en lecture et de même propriétaire, 
 * on ajoute le verrou, sinon si les blocs ne se chevauchent pas, on ajoute le verrou
*/
static int rl_fcntl_rlock(rl_descriptor *lfd, struct flock *lck) {

	rl_lock new_lock = {
		.next_lock = -1,
		.starting_offset = lck->l_start,
		.len = lck->l_len,
		.type = lck->l_type,
		.nb_owners = 1};

	new_lock.lock_owners[0] = (owner) {
		.proc = getpid(),
		.des = lfd->d};

	rl_open_file *file = lfd->f;

	pthread_mutex_lock(&file->mutex);
	while (file->busy) {
		pthread_cond_wait(&file->cond, &file->mutex);
	}
	file->busy = true;

	// s'il n'y a pas de lock on ajoute
	if (file->first == -2) {
		file->lock_table[0] = new_lock;
		file->first = 0;

		file->busy = false;
		pthread_mutex_unlock(&file->mutex);
		pthread_cond_signal(&file->cond);

		return 0;
	}

	rl_lock *curr_lock = &file->lock_table[file->first];

	// tant que le verrou courant n'est pas le dernier
	while (curr_lock->next_lock >= 0) {

		// si le nouveau verrou chevauche un WRITE lock -> error
		if (lock_overlap(new_lock, *curr_lock) && curr_lock->type == F_WRLCK) {
			printf("chevauchement avec un verrou en écriture, posée d'un verrou en lecture impossible\n");
			errno = EAGAIN;
			file->busy = false;
			pthread_mutex_unlock(&file->mutex);
			pthread_cond_signal(&file->cond);
			return -1;
		}

		curr_lock = &file->lock_table[curr_lock->next_lock];
	}

	// on cherche le premier espace libre
	int i = 0;
	while (file->lock_table[i].next_lock != -2 && i < NB_LOCKS) {
		i++;
	}

	// on ajoute le verrou
	file->lock_table[i] = new_lock;
	// on met à jour le next_lock du dernier verrou
	curr_lock->next_lock = i;

	// on fusionne les verrous qui peuvent l'être
	merge_locks(lfd);

	// on libère le mutex
	file->busy = false;
	pthread_mutex_unlock(&file->mutex);
	pthread_cond_signal(&file->cond);

	return 0;
}

// ## RL_FTNCL
int rl_fcntl(rl_descriptor lfd, int cmd, struct flock *lck) {
	switch (lck->l_whence) {

	case SEEK_SET:
		break;
	case SEEK_CUR:
		lck->l_whence = SEEK_SET;
		if ((lck->l_start = lseek(lfd.d, 0, SEEK_CUR)) == -1)
			PANIC_EXIT("LSEEK");
		break;
	case SEEK_END:
		lck->l_whence = SEEK_SET;
		if ((lck->l_start = lseek(lfd.d, 0, SEEK_END)) == -1)
			PANIC_EXIT("LSEEK");
		break;
	}

	switch (cmd) {

	case F_SETLK:
		if (lck->l_type == F_UNLCK) {
			printf("tentative suppression verrou, segment : [%li, %li]\n", lck->l_start, lck->l_start + lck->l_len);
			return rl_fcntl_unlock(&lfd, lck);
		}
		else if (lck->l_type == F_WRLCK) {
			printf("tentative pose verrou écriture, segment : [%li, %li]\n", lck->l_start, lck->l_start + lck->l_len);
			return rl_fcntl_wlock(&lfd, lck);
		} else if (lck->l_type == F_RDLCK) {
			printf("tentative pose verrou lecture, segment : [%li, %li]\n", lck->l_start, lck->l_start + lck->l_len);
			return rl_fcntl_rlock(&lfd, lck);
		}
		break;
	case F_SETLKW:
		break;
	default:
		errno = EAGAIN;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}

void rl_debug() {

	for (int i = 0; i < all_files.nb_files; i++) {

		rl_open_file *file = all_files.tab_open_files[i];

		if (file->first == -2) {
			puts("No locks on file");
			return;
		}

		printf("Etat des verrous :\n\tFile n°%d\n", i);

		rl_lock *curr_lock = &file->lock_table[file->first];
		do {

			printf("\t%s lock (%ld -> %ld)\n\tOwners:\n", curr_lock->type == F_WRLCK ? "WRITE" : "READ", curr_lock->starting_offset, curr_lock->starting_offset + curr_lock->len);

			for (size_t j = 0; j < curr_lock->nb_owners; j++) {

				printf("\t\t%d, %d\n", curr_lock->lock_owners[j].des, curr_lock->lock_owners[j].proc);
			}

			if (curr_lock->next_lock == -1) {
				break;
			}

			curr_lock = &file->lock_table[curr_lock->next_lock];
		} while (curr_lock->next_lock >= -1);
	}
}