CC              = gcc
RM              = rm
CFLAGS          = -Wall -std=gnu99
DEBUG_FLAGS     = -g

BIN_DIR         = bin

S_CHANNEL       = channel.c

S_BRUTE         = brute_force.c

S_MANDEL        = mandelbrot.c
FLAG_MANDEL_1   = -O3 -ffast-math -pthread `pkg-config --cflags gtk+-3.0`
FLAG_MANDEL_2   = `pkg-config --libs gtk+-3.0` -lm
EXEC_MANDEL     = $(BIN_DIR)/mandelbrot

S_PROD_CONSO    = prod_conso.c
FLAG_PROD_CONSO = -pthread
EXEC_PROD_CONSO = $(BIN_DIR)/prod_conso

S_BF_TEST       = bf_test.c
FLAG_BF         = -pthread
EXEC_BF_TEST    = $(BIN_DIR)/brute_force



mandelbrot:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(FLAG_MANDEL_1) $(S_MANDEL) $(S_CHANNEL) $(FLAG_MANDEL_2) -o $(EXEC_MANDEL)

prod_conso:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(FLAG_PROD_CONSO) $(S_PROD_CONSO) $(S_CHANNEL) -o $(EXEC_PROD_CONSO)

brute_force:
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) $(FLAG_BF) $(S_BF_TEST) $(S_BRUTE) $(S_CHANNEL) -o $(EXEC_BF_TEST)

clean:
	$(RM) -r $(BIN_DIR)