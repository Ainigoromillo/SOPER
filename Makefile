objects=main.o pow.o
program=miner
CC = gcc
CFLAGS = -ansi -pedantic -Wall -g

ARG1 = 10
ARG2 = 5
ARG3 = 2

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
	./miner $(ARG1) $(ARG2) $(ARG3)

runv:
	valgrind --leak-check=full ./miner $(ARG1) $(ARG2) $(ARG3)