#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
       #include <fcntl.h>

#include "rl_lock_library.h"

int main(void) {

    rl_descriptor desc = rl_open("concu_file", O_RDONLY | O_CREAT, 0644);
    if(desc.d < 0 && desc.f == NULL) {
        perror("rl_open");
        goto error;
    }

    struct flock lock = {
		.l_len = 100,
		.l_start = 0,
		.l_whence = SEEK_SET,
		.l_type = F_WRLCK
	};

    int ret;

    do {

        ret = rl_fcntl(desc, F_SETLK, &lock);
        if(ret < 0) {
            perror("rl_fcntl");
        }

        sleep(1);

    } while(ret < 0 && errno == EAGAIN);

    puts("verrou posé");

    sleep(10);

    puts("fin sleep");

    rl_close(desc);

    puts("fin close");

    return EXIT_SUCCESS;

error:
    return EXIT_FAILURE;
}