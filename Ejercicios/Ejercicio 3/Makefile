EXE=main
CC=gcc
OBJ=$(EXE).o

$(EXE): $(OBJ)
	$(CC) $(OBJ) -o $(EXE) 

%.o: %.c 
	$(CC) -c $< -o $@