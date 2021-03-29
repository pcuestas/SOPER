EXE=conc_cycle
CC=gcc

LIB=-lpthread

$(EXE): $(EXE).c
	$(CC) $(EXE).c $(LIB) -o $@

run: $(EXE)
	@echo "------------"
	./$(EXE)

clean:
	$(RM) ./$(EXE)