CC=gcc
HEADER=stream.h
LIB=-lpthread -lrt
OBJ=stream_func.o
ALL=stream-ui stream-server stream-client


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