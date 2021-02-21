EXE=main
CC=gcc
OBJ=$(EXE).o

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE) 
	rm *.o

run: $(EXE)
	./$(EXE)

%.o: %.c 
	$(CC) -c $< -o $@