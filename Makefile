CFLAGS=-Wall -Wextra -Werror -g -std=gnu17 -Isrc/include
LDFLAGS=-lrt -pthread

SRC_DIR=src
BUILD_DIR=build

SRC=$(wildcard $(SRC_DIR)/*.c)
OBJ=$(SRC:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
PRGM=rl_lib_main

.PHONY: 	all

all:	$(PRGM)

$(PRGM):	$(OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR):
	@mkdir -p $@

clean:
	rm -rf build $(PRGM)

