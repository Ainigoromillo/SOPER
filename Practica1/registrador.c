#include "registrador.h"

#include "pow.h"
#include <time.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>


void funcionaldadRegistrador(int minero_escribe[2], int registrador_escribe[2]){
    int round;
    char *pointer, buffer[1024];
    int fd;
    long target, solution;
    int yesNo[2];
    int rondas_verificadas;
    char *status;

    sprintf(buffer, "%jd.log", (intmax_t)getppid());

    /*Se cierran los pipes que no necesitaremos y se abre el descriptor de
     * fichero donde escribiremos los resultados*/
    close(minero_escribe[1]);      /*minero escribe (write) */
    close(registrador_escribe[0]); /*registrador escribe (read) */

    if ((fd = open(buffer, O_CREAT | O_TRUNC | O_RDWR,
                   S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
    {
      perror("open");
      close(minero_escribe[0]);
      close(registrador_escribe[1]);

      printf("Register exited with status 1\n");
      exit(EXIT_FAILURE);
    }

    while (read(minero_escribe[0], buffer, sizeof(buffer)) > 0)
    {

      pointer = strtok(buffer, "|\n\r");
      round = atoi(pointer);
      pointer = strtok(NULL, "|\n\r");
      target = atol(pointer);
      pointer = strtok(NULL, "|\n\r");
      solution = atol(pointer);
      pointer = strtok(NULL, "|\n\r");
      yesNo[0] = atoi(pointer);
      pointer = strtok(NULL, "|\n\r");
      yesNo[1] = atoi(pointer);
      pointer = strtok(NULL, "|\n\r");
      rondas_verificadas = atoi(pointer);
      status = strtok(NULL, "|\n\r");

      /*Escribe los resultados en el fichero*/
      dprintf(fd,
              "Id:%d \n"
              "Winner:%jd \n"
              "Target:%d \n"
              "Solution: %ld (%s)\n"
              "Votes: %d/%d \n"
              "Wallets: %jd:%d\n\n",
              round, (intmax_t)getppid(), target, solution, status, yesNo[0], yesNo[0] + yesNo[1],
              (intmax_t)getppid(), rondas_verificadas);

      /**Manda señal de que ya ha escrito en el fichero */
      write(registrador_escribe[1], buffer, strlen(buffer) + 1);
    }

}