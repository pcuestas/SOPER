######################################################
#
# file: Makefile. SOPER, Práctica 3.
#
# autores: Pablo Cuesta Sierra y Álvaro Zamanillo Sáez
#
######################################################

CC=gcc
HEADER=miner.h
LIB=-lpthread -lrt
OBJ=miner.o
ALL=mr_miner


all : $(ALL)

%.o: %.c $(HEADER)
	@echo "# -------------------------------"
	@echo "# Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(ALL) : % : %.o $(OBJ)
	@echo "# -------------------------------"
	@echo "# Linking $@" 
	$(CC) $^ $(LIB) -o  $@

clean:
	$(RM) *.o $(ALL)