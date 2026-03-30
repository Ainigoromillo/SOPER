objects=main.o pow.o
program=miner
CC = gcc
CFLAGS = -ansi -pedantic -Wall -g

N_THREADS = 40
N_SECS = 5

all: $(program) 


miner: $(objects)
	$(CC) -o $@ $^ -lpthread

minerRapido: mainRapido.o pow.o
	$(CC) -o $@ $^c-lpthread

mainRapido: mainRapido.c
	$(CC) -o $@ -c $<

main.o: main.c
	$(CC) -o $@ -c $< 

pow.o: pow.c
	$(CC) -o $@ -c $< 


.PHONY: clean cleanLog run all
clean:
	rm *.o $(program) *.log

cleanLog:
	rm *.log

cleanRapido:
	rm mainRapido.o mainRapido

run:
	./miner $(N_SECS) $(N_THREADS)
	./miner $(N_SECS) $(N_THREADS)

runv:
	valgrind --leak-check=full --track-origins=yes -s ./miner $(TARGET) $(ROUNDS) $(THREADS)
