######################################################
#
# file: Makefile. SOPER, Práctica 4.
#
# autores: Pablo Cuesta Sierra y Álvaro Zamanillo Sáez
#
######################################################

CC=gcc
CFLAGS=-pedantic -Wall -g
HEADERS=miner.h mr_miner.h
LIB=-lpthread -lrt
OBJ_MINER=miner.o mr_miner.o mr_functions.o
MINER=mr_miner

all : $(MINER)

%.o: %.c $(HEADERS)
	@echo "# -------------------------------"
	@echo "# Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(MINER) : $(OBJ_MINER)
	@echo "# -------------------------------"
	@echo "# Linking $@" 
	$(CC) $^ $(LIB) -o  $@

clean:
	$(RM) *.o $(MINER)