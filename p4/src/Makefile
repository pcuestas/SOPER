######################################################
#
# file: Makefile. SOPER, Práctica 4.
#
# autores: Pablo Cuesta Sierra y Álvaro Zamanillo Sáez
#
######################################################

CC=gcc
CFLAGS=
HEADERS=miner.h mr_miner.h mr_monitor.h mr_util.h
LIB=-lpthread -lrt

MINER=mr_miner
MONITOR=mr_monitor
ALL=$(MINER) $(MONITOR)

OBJ_COMMON=miner.o mr_util.o
OBJ_MINER=$(OBJ_COMMON) mr_miner_func.o mr_miner_workers.o
OBJ_MONITOR=$(OBJ_COMMON) mr_monitor_func.o

all : $(ALL)

%.o: %.c $(HEADERS)
	@echo "# -------------------------------"
	@echo "# Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(MINER): % : %.o $(OBJ_MINER)
	@echo "# -------------------------------"
	@echo "# Linking $@" 
	$(CC) $^ $(LIB) -o  $@

$(MONITOR): % : %.o $(OBJ_MONITOR)
	@echo "# -------------------------------"
	@echo "# Linking $@" 
	$(CC) $^ $(LIB) -o  $@

clean:
	$(RM) *.o $(ALL) *.log


remove:
	rm /dev/shm/sem.mr_mutex \
	   /dev/shm/block \
	   /dev/shm/netdata \
	   /dev/mqueue/mr_mq_monitor