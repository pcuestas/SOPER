EXE=conc_cycle
CC=gcc

LIB=-lpthread

all: $(EXE)

%: %.c
	$(CC) $< $(LIB) -o $@

clean:
	$(RM) ./$(EXE)
