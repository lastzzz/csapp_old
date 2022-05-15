CC = /usr/bin/gcc-7
CFLAGS = -Wall -g -O2 -Werror -std=gnu99 -Wno-unused-function

BIN_HARDWARE = ./bin/test_hardware
BIN_LINK = ./bin/test_elf

SRC_DIR = ./src

# debug
COMMON = $(SRC_DIR)/common/print.c $(SRC_DIR)/common/convert.c

# hardware

CPU = $(SRC_DIR)/hardware/cpu/mmu.c $(SRC_DIR)/hardware/cpu/isa.c 
MEMORY = $(SRC_DIR)/hardware/memory/dram.c
LINK = $(SRC_DIR)/linker/parseElf.c $(SRC_DIR)/linker/staticlink.c
# ALGORITHM = $(SRC_DIR)/algorithm/array.c $(SRC_DIR)/algorithm/hashtable.c $(SRC_DIR)/algorithm/linkedlist.c $(SRC_DIR)/algorithm/trie.c

# main
TEST_HARDWARE = $(SRC_DIR)/tests/test_hardware.c
TEST_LINK = $(SRC_DIR)/tests/test_elf.c



.PHONY: hardware

hardware:
	$(CC) $(CFLAGS) -I$(SRC_DIR) $(COMMON) $(CPU) $(MEMORY) $(DISK) $(TEST_HARDWARE) -o $(BIN_HARDWARE)
	./$(BIN_HARDWARE)

.PHONY: link

link:
	$(CC) $(CFLAGS) -I$(SRC_DIR) $(COMMON) $(CPU) $(MEMORY) $(LINK) $(TEST_LINK) -o $(BIN_LINK)
	./$(BIN_LINK)

clean:
	rm -f *.o *~ $(EXE_HARDWARE) $(EXE_LINK)
