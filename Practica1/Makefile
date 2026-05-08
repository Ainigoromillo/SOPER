
program=miner
objectsMiner=main.o pow.o minero.o registrador.o
programs=miner monitor
CC = gcc
#CFLAGS = -ansi -pedantic -Wall -g
CFLAGS = -Wall -g
LIBS = -lpthread -lrt

N_THREADS = 1
N_SECS = 5

all: $(programs)


miner: $(objectsMiner)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

monitor: monitor.o pow.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

minerRapido: mainRapido.o pow.o
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

mainRapido: mainRapido.c
	$(CC) $(CFLAGS) -o $@ -c $<

main.o: main.c resources.h pow.h
	$(CC) $(CFLAGS) -o $@ -c $<

pow.o: pow.c
	$(CC) $(CFLAGS) -o $@ -c $<

monitor.o: monitor.c resources.h pow.h 
	$(CC) $(CFLAGS) -o $@ -c $<

minero.o: minero.c minero.h resources.h
	$(CC) $(CFLAGS) -o $@ -c $<

registrador.o: registrador.c registrador.h resources.h
	$(CC) $(CFLAGS) -o $@ -c $<


monitor_alternativo: monitor_alternativo.o pow.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

monitor_alternativo.o: monitor_alternativo.c resources.h pow.h
	$(CC) $(CFLAGS) -o $@ -c $<


monitor_cola: monitor_cola.o pow.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

monitor_cola.o: monitor_cola.c resources.h pow.h
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean cleanLog run all
.PHONY: clean cleanLog clean_sem clean_mq cleanAll run all
cleanAll:
	make clean
	make cleanLog
	make clean_sem
	make clean_mq

clean:
	rm *.o $(programs)
	rm -f *.pid
	rm -f *.tgt
	rm -f *.vot

cleanLog:
	rm -f *.log

cleanRapido:
	rm mainRapido.o mainRapido

clean_mq:
	rm -f /dev/mqueue/miner_comprobador_mq

clean_all:
	$(MAKE) clean_mq
	$(MAKE) clean_sem
	$(MAKE) clean_shm

clean_sem:
	rm -f /dev/shm/sem.mutex_pids_sem
	rm -f /dev/shm/sem.mutex_target
	rm -f /dev/shm/sem.ganador_sem
	rm -f /dev/shm/sem.mutex_voting
	rm -f /dev/shm/sem.mutex_wallets


clean_shm:
	rm -f /dev/shm/Pids.pid
	rm -f /dev/shm/Voting.vot
	rm -f /dev/shm/Target.tgt
	rm -f /dev/shm/systemFile
	rm -f /dev/shm/Wallets.txt

run_monitor:
	./monitor 100 100

run_mineros:
	./miner 12 4 &./miner 12 4


	

