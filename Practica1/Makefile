objects=main.o pow.o
program=miner
CC = gcc
CFLAGS = -ansi -pedantic -Wall -g

TARGET = 40
ROUNDS = 50
THREADS = 1

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


.PHONY: clean run all
clean:
	rm *.o $(program) minerRapido *.log

cleanLog:
	rm *.log
run:
	./miner $(TARGET) $(ROUNDS) $(THREADS)

runv:
	valgrind --leak-check=full ./miner $(TARGET) $(ROUNDS) $(THREADS)