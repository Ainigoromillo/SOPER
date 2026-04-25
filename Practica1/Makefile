objects=main.o pow.o
program=miner
CC = gcc
#CFLAGS = -ansi -pedantic -Wall -g
CFLAGS = -Wall -g
LIBS = -lpthread -lrt

N_THREADS = 1
N_SECS = 5

all: $(program) 


miner: $(objects)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

minerRapido: mainRapido.o pow.o
	$(CC) $(CFLAGS) -o $@ $^c-lpthread

mainRapido: mainRapido.c
	$(CC) $(CFLAGS) -o $@ -c $<

main.o: main.c
	$(CC) $(CFLAGS) -o $@ -c $<

pow.o: pow.c
	$(CC) $(CFLAGS) -o $@ -c $<


.PHONY: clean cleanLog clean_sem cleanAll run all
cleanAll:
	make clean
	make cleanLog
	make clean_sem

clean:
	rm *.o $(program) 
	rm -f *.pid
	rm -f *.tgt
	rm -f *.vot

cleanLog:
	rm -f *.log

cleanRapido:
	rm mainRapido.o mainRapido

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


