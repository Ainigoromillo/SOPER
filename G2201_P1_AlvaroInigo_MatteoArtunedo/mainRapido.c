#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include "pow.h"

int finished = 0;

typedef struct
{
    long n_hilo;
    long n_valores;
    long target;
} ArgsSolucion;

int main(int argc, char *argv[])
{
    long n_rounds = 0, target_ini = 0, pid = 0;
    int i = 0;
    int n_threads = 0;
    int minero_escribe[2];
    int registrador_escribe[2];
    int fd;

    long int solution;

    char validado[] = "validated";

    char buffer[1024];


    /*Tratamiento de los argumentos de entrada*/
    if (argc != 4)
    {
        printf("Not enough arguments for the program\n");
        return EXIT_FAILURE;
    }
    target_ini = atoi(argv[1]);
    n_rounds = atoi(argv[2]);
    n_threads = atoi(argv[3]);

    /*Apertura del pipe*/
    pipe(minero_escribe);
    pipe(registrador_escribe);

    /*División de los procesos*/
    pid = fork();
    if (pid < 0)
    {
        perror("Error en el fork\n");
        return EXIT_FAILURE;
    }
    if (pid == 0)
    {
        int round;
        long solution;
        char *pointer;

        sprintf(buffer, "%jd.log", (intmax_t)getppid());
        /*Se cierran los pipes pertinentes y apertura de descriptor de fichero*/

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
            round = atoi(pointer) + 1;
            pointer = strtok(NULL, "|\n\r");
            target_ini = atol(pointer);
            pointer = strtok(NULL, "|\n\r");
            solution = atol(pointer);

            if (solution < 0)
            {
                printf("Register exited with status 0\n");
                exit(EXIT_SUCCESS);
            }
            dprintf(fd, "Id:%d \n"
                        "Winner:%jd \n"
                        "Target:%ld \n"
                        "Solution: %ld (%s)\n"
                        "Votes: %d/%d \n"
                        "Wallets: %jd:%d\n\n",
                    round, (intmax_t)getppid(), target_ini, solution, validado, round, round, (intmax_t)getppid(), round);

            /**Manda señal de que ya ha escrito en el fichero */
            write(registrador_escribe[1], buffer, sizeof(buffer));
        }
        close(minero_escribe[0]);
        close(registrador_escribe[1]);
        printf("Register exited with status 0\n");
        exit(EXIT_SUCCESS);
    }
    else
    {
        /**Se cierran pipes pertinentes*/
        close(registrador_escribe[1]); /*registrador escribe (escritura)*/
        close(minero_escribe[0]);      /*minero escribe (lectura)*/
        /*Ejecutamos el código del minero*/
        for (i = 0; i < n_rounds; i++)
        {
            
            solution = (((target_ini - 24849)* 1938508) % POW_LIMIT + POW_LIMIT) % POW_LIMIT;




            sprintf(buffer, "%d|%ld|%ld\n", i, target_ini, solution);

            write(minero_escribe[1], buffer, strlen(buffer) + 1);


            printf("Solution accepted: %ld------->%ld\n", target_ini, solution);

            target_ini = solution;
            /**Leemos la señal del registrador para continuar con la siguiente ronda */
            if (read(registrador_escribe[0], buffer, sizeof(buffer)) <= 0)
            {
                printf("Miner exited with status 0\n");
                wait(NULL);
                return EXIT_SUCCESS;
            }
            finished = 0;
        }

        /**Numero de rondas terminado, mandamos señal de final y liberamos memoria */
        sprintf(buffer, "%d|%ld|%ld\n", i, target_ini, (long)-1);
        write(minero_escribe[1], buffer, strlen(buffer) + 1);
        wait(NULL);
    }
    printf("Miner exited with status 0\n");
    return EXIT_SUCCESS;
}