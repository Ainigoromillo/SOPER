objects=main.o pow.o
program=miner
CC = gcc
CFLAGS = -ansi -pedantic -Wall -g

TARGET = 40
ROUNDS = 5
THREADS = 2

all: $(program) 


miner: $(objects)
	$(CC) -o $@ $^

main.o: main.c
	$(CC) -o $@ -c $< 

pow.o: pow.c
	$(CC) -o $@ -c $< 


.PHONY: clean run all
clean:
	rm $(objects) $(program)

run:
	./miner $(TARGET) $(ROUNDS) $(THREADS)

runv:
	valgrind --leak-check=full ./miner $(TARGET) $(ROUNDS) $(THREADS)