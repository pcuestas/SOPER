######################################################
#
# file: Makefile. SOPER, Práctica 2.
#
# autores: Pablo Cuesta Sierra y Álvaro Zamanillo Sáez
#
######################################################

EXE=conc_cycle
CC=gcc
LIB=-lpthread

all: $(EXE)

%: %.c
	$(CC) $< $(LIB) -o $@

clean:
	$(RM) ./$(EXE)
