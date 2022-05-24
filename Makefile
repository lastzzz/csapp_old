CC = /usr/bin/gcc-7
# 加-O2 会警告linkedlist.c里的东西
# CFLAGS = -Wall -g -O2 -Werror -std=gnu99 -Wno-unused-function
CFLAGS = -Wall -g   -O0 -Werror -std=gnu99 -Wno-unused-function

BIN_HARDWARE = ./bin/test_hardware
BIN_LINK = ./bin/test_elf
LINKSO = ./bin/staticlinker.so
EXE_LINKSO = ./bin/link
BIN_MESI = ./bin/mesi
BIN_FALSE_SHARING = ./bin/false_sharing
BIN_MALLOC = ./bin/malloc

SRC_DIR = ./src

# debug
COMMON = $(SRC_DIR)/common/print.c $(SRC_DIR)/common/convert.c $(SRC_DIR)/common/tagmalloc.c $(SRC_DIR)/common/cleanup.c

# hardware

CPU = $(SRC_DIR)/hardware/cpu/mmu.c $(SRC_DIR)/hardware/cpu/isa.c $(SRC_DIR)/hardware/cpu/sram.c
MEMORY = $(SRC_DIR)/hardware/memory/dram.c $(SRC_DIR)/hardware/memory/swap.c 
LINK = $(SRC_DIR)/linker/parseElf.c $(SRC_DIR)/linker/staticlink.c
ALGORITHM = $(SRC_DIR)/algorithm/array.c $(SRC_DIR)/algorithm/hashtable.c $(SRC_DIR)/algorithm/linkedlist.c $(SRC_DIR)/algorithm/trie.c
MALLOC = $(SRC_DIR)/malloc/mem_alloc.c

# main
TEST_HARDWARE = $(SRC_DIR)/tests/test_hardware.c
TEST_LINK = $(SRC_DIR)/tests/test_elf.c
TEST_MESI = $(SRC_DIR)/tests/mesi.c
TEST_FALSE_SHARING = $(SRC_DIR)/tests/false_sharing.c
TEST_MALLOC = $(SRC_DIR)/tests/test_malloc.c


# ---------------------hardware----------------------------------------------------------------------

.PHONY: hardware

hardware:
	$(CC) $(CFLAGS) -I$(SRC_DIR) -DUSE_NAVIE_VA2PA $(COMMON) $(CPU) $(MEMORY) $(DISK) $(ALGORITHM) $(TEST_HARDWARE) -o $(BIN_HARDWARE)
	./$(BIN_HARDWARE)

# ---------------------link---------------------------------------------------------------------------

.PHONY: link

link:
	$(CC) $(CFLAGS) -I$(SRC_DIR) $(COMMON) $(CPU) $(MEMORY) $(ALGORITHM) $(LINK) $(TEST_LINK) -o $(BIN_LINK)
	./$(BIN_LINK)

# ---------------------linkso---------------------------------------------------------------------------

.PHONY: linkso

linkso:
	$(CC) $(CFLAGS) -I$(SRC_DIR) -shared -fPIC $(COMMON) $(ALGORITHM)  $(LINK) -o $(LINKSO)


# ---------------------linkso_link---------------------------------------------------------------------------

.PHONY: linkso_link

linkso_link:
	$(CC) $(CFLAGS) -I$(SRC_DIR)  $(SRC_DIR)/common/convert.c $(ALGORITHM) $(SRC_DIR)/linker/linker.c -ldl -o $(EXE_LINKSO)


# ---------------------mesi---------------------------------------------------------------------------
.PHONY: mesi

mesi:
	$(CC)  -Wall -g -O0 -Werror -std=gnu99 -Wno-unused-but-set-variable -I$(SRC_DIR) $(TEST_MESI) -o $(BIN_MESI)
	./$(BIN_MESI)
# ---------------------false_sharing---------------------------------------------------------------------------

.PHONY: false_sharing

false_sharing:
	$(CC) -Wall -g -O0 -Werror -std=gnu99 -Wno-unused-but-set-variable -Wno-unused-variable -I$(SRC_DIR) -pthread $(TEST_FALSE_SHARING) -o $(BIN_FALSE_SHARING)
	./$(BIN_FALSE_SHARING)

.PHONY: malloc

malloc:
	$(CC) -Wall -g -O0 -Werror -std=gnu99 -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-function -I$(SRC_DIR) $(COMMON) $(ALGORITHM) $(MALLOC) $(TEST_MALLOC) -o $(BIN_MALLOC)
	./$(BIN_MALLOC)


.PHONY: mallocc
mallocc:
	$(CC) -Wall -g -O0 -Werror -std=gnu99 -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-function -I$(SRC_DIR) -DDEBUG_MALLOC ./src/algorithm/linkedlist.c ./src/malloc/mem_alloc.c -o ./bin/malloc
	./bin/malloc

clean:
	rm -f *.o *~ $(EXE_HARDWARE) $(EXE_LINK) $(LINKSO) $(BIN_MESI)
