CC=gcc
CFLAGS= -g -Wall -Wextra -Werror -I include/ 
BUILD_DIR=build

all: library read_lock read_lock_sleep1 read_lock_sleep2 write_lock fork_read_lock test1 concu_block

library: $(BUILD_DIR)/rl_lock_library.o

read_lock: $(BUILD_DIR)/read_lock.o
	$(CC) $(CFLAGS) -o $@ $^ $(BUILD_DIR)/rl_lock_library.o -lrt

read_lock_sleep1: $(BUILD_DIR)/read_lock_sleep1.o
	$(CC) $(CFLAGS) -o $@ $^ $(BUILD_DIR)/rl_lock_library.o -lrt

read_lock_sleep2 : $(BUILD_DIR)/read_lock_sleep2.o
	$(CC) $(CFLAGS) -o $@ $^ $(BUILD_DIR)/rl_lock_library.o -lrt

write_lock: $(BUILD_DIR)/write_lock.o
	$(CC) $(CFLAGS) -o $@ $^ $(BUILD_DIR)/rl_lock_library.o -lrt

fork_read_lock: $(BUILD_DIR)/fork_read_lock.o
	$(CC) $(CFLAGS) -o $@ $^ $(BUILD_DIR)/rl_lock_library.o -lrt

test1: $(BUILD_DIR)/test1.o
	$(CC) $(CFLAGS) -o $@ $^ $(BUILD_DIR)/rl_lock_library.o -lrt

concu_block: $(BUILD_DIR)/lock_concu.o
	$(CC) $(CFLAGS) -o $@ $^ $(BUILD_DIR)/rl_lock_library.o -lrt

$(BUILD_DIR)/%.o : src/%.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -rf library build/*.o read_lock read_lock_sleep1 read_lock_sleep2 write_lock fork_read_lock test1 fichier_verrous concu_file concu_block
