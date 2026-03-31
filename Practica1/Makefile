objects=main.o pow.o
program=miner
CC = gcc
#CFLAGS = -ansi -pedantic -Wall -g
CFLAGS = -Wall -g

N_THREADS = 10
N_SECS = 5

all: $(program) 


miner: $(objects)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread

minerRapido: mainRapido.o pow.o
	$(CC) $(CFLAGS) -o $@ $^c-lpthread

mainRapido: mainRapido.c
	$(CC) $(CFLAGS) -o $@ -c $<

main.o: main.c
	$(CC) $(CFLAGS) -o $@ -c $<

pow.o: pow.c
	$(CC) $(CFLAGS) -o $@ -c $<


.PHONY: clean cleanLog run all
clean:
	rm *.o $(program) *.log

cleanLog:
	rm *.log

cleanRapido:
	rm mainRapido.o mainRapido

run:
	rm -f *.pid
	rm -f *.log
	./miner 8 $(N_THREADS) & ./miner 15 $(N_THREADS)

runv:
	rm *.pid
	rm *.log
	valgrind --leak-check=full --track-origins=yes -s ./miner $(N_SECS) $(N_THREADS) & \\
		\\ valgrind --leak-check=full --track-origins=yes -s ./miner $(N_SECS) $(N_THREADS)


