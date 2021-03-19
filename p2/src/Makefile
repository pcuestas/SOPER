
EXE=conc_cycle
CC=gcc
OBJ=$(EXE).o
LIB=-lpthread
CFLAGS=#-pedantic -g #### revisar banderas

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) $(LIB) -o $(EXE) 
	rm *.o

run: $(EXE)
	@echo "------------"
	./$(EXE)

%.o: %.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) ./$(EXE) *.o

test: 
	@echo $(LIB)