objects=main.o pow.o
program=miner
CC = gcc
CFLAGS = -ansi -pedantic -Wall -g

TARGET = 40
ROUNDS = 50
THREADS = 5

all: $(program) 


miner: $(objects)
	$(CC) -o $@ $^

minerRapido: mainRapido.o pow.o
	$(CC) -o $@ $^

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
	./miner $(TARGET) $(ROUNDS) $(THREADS)

runv:
	valgrind --leak-check=full --track-origins=yes -s ./miner $(TARGET) $(ROUNDS) $(THREADS)