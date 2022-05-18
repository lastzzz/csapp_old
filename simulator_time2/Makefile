CC = /usr/bin/gcc-7
# 加-O2 会警告linkedlist.c里的东西
# CFLAGS = -Wall -g -O2 -Werror -std=gnu99 -Wno-unused-function
CFLAGS = -Wall -g   -O0 -Werror -std=gnu99 -Wno-unused-function

BIN_HARDWARE = ./bin/test_hardware
BIN_LINK = ./bin/test_elf
LINKSO = ./bin/staticlinker.so
EXE_LINKSO = ./bin/link
BIN_MESI = ./bin/mesi

SRC_DIR = ./src

# debug
COMMON = $(SRC_DIR)/common/print.c $(SRC_DIR)/common/convert.c $(SRC_DIR)/common/tagmalloc.c $(SRC_DIR)/common/cleanup.c

# hardware

CPU = $(SRC_DIR)/hardware/cpu/mmu.c $(SRC_DIR)/hardware/cpu/isa.c 
MEMORY = $(SRC_DIR)/hardware/memory/dram.c
LINK = $(SRC_DIR)/linker/parseElf.c $(SRC_DIR)/linker/staticlink.c
ALGORITHM = $(SRC_DIR)/algorithm/array.c $(SRC_DIR)/algorithm/hashtable.c $(SRC_DIR)/algorithm/linkedlist.c $(SRC_DIR)/algorithm/trie.c

# main
TEST_HARDWARE = $(SRC_DIR)/tests/test_hardware.c
TEST_LINK = $(SRC_DIR)/tests/test_elf.c




.PHONY: hardware

hardware:
	$(CC) $(CFLAGS) -I$(SRC_DIR) $(COMMON) $(CPU) $(MEMORY) $(DISK) $(ALGORITHM) $(TEST_HARDWARE) -o $(BIN_HARDWARE)
	./$(BIN_HARDWARE)


.PHONY: link

link:
	$(CC) $(CFLAGS) -I$(SRC_DIR) $(COMMON) $(CPU) $(MEMORY) $(ALGORITHM) $(LINK) $(TEST_LINK) -o $(BIN_LINK)
	./$(BIN_LINK)


.PHONY: linkso

linkso:
	$(CC) $(CFLAGS) -I$(SRC_DIR) -shared -fPIC $(COMMON) $(ALGORITHM)  $(LINK) -o $(LINKSO)

.PHONY: linkso_link

linkso_link:
	$(CC) $(CFLAGS) -I$(SRC_DIR)  $(SRC_DIR)/common/convert.c $(ALGORITHM) $(SRC_DIR)/linker/linker.c -ldl -o $(EXE_LINKSO)


.PHONY: mesi

mesi:
	$(CC)  -Wall -g -O0 -Werror -std=gnu99 -Wno-unused-but-set-variable -I$(SRC_DIR) $(SRC_DIR)/hardware/cpu/mesi.c -o $(BIN_MESI)
	./$(BIN_MESI)


clean:
	rm -f *.o *~ $(EXE_HARDWARE) $(EXE_LINK) $(LINKSO)
