######################################################
#
# file: Makefile. SOPER, Práctica 2.
#
# autores: Pablo Cuesta Sierra y Álvaro Zamanillo Sáez
#
######################################################


ALL=stream-ui stream-server stream-client
CC=gcc
LIB=-lpthread -lrt
HEADERS=stream.h

all: $(ALL)

%: %.c $(HEADERS)
	$(CC) $< $(LIB) -o $@

clean:
	$(RM) $(ALL)

