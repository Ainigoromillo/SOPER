objectsMiner=main.o pow.o
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

monitor:	monitor.o
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

minerRapido: mainRapido.o pow.o
	$(CC) $(CFLAGS) -o $@ $^c-lpthread

mainRapido: mainRapido.c
	$(CC) $(CFLAGS) -o $@ -c $<

main.o: main.c resources.h pow.h
	$(CC) $(CFLAGS) -o $@ -c $<

monitor.o: monitor.c resources.h pow.h
	$(CC) $(CFLAGS) -o $@ -c $<

pow.o: pow.c
	$(CC) $(CFLAGS) -o $@ -c $<


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
	rm -f /dev/mqueue/comprobador_monitor_mq
	rm -f /dev/mqueue/miner_comprobador_mq


clean_sem:
	rm -f /dev/shm/sem.mutex_pids_sem
	rm -f /dev/shm/sem.mutex_target
	rm -f /dev/shm/sem.ganador_sem
	rm -f /dev/shm/sem.mutex_voting
	rm -f /dev/shm/Pids.pid
	rm -f /dev/shm/Voting.vot
	rm -f /dev/shm/Target.tgt
	rm -f Pids.pid Target.tgt Voting.vot
run:
	./miner 7 1 & ./miner 6 1 & ./miner 6 1 & ./miner 6 1 & ./miner 6 1 & ./miner 6 1 & ./miner 6 1  & ./miner 6 1  & ./miner 6 1

runv:

	valgrind --leak-check=full --track-origins=yes -s ./miner $(N_SECS) $(N_THREADS) & valgrind --leak-check=full --track-origins=yes -s ./miner $(N_SECS) $(N_THREADS)


