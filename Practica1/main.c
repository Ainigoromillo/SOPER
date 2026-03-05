/**
 * @file main.c
 * @author Alvaro Iñigo y Matteo Artuñedo
 * @brief implementa un programa en el que varios mineros (hilos) tendrán que buscar la preimagen de un valor por una función hash y comunicarse con un registrador para escribirlo.
 * @version 0.2
 * @date 2026-02-28
 * 
 */
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

#define SOLUTION_NOT_FOUND -1 /**Valor que devuelven los hilos si no han encontrado una solución para el target */

/**En esta iteración, determinaremos que una solución será rechazada si es múltiplo de diez. Este criterio será modificado en iteraciones futuras */
#define VALIDATE(solution)((solution)%10==0 ? (0) : (1)) 

/**
 * @brief Estructura que almacena la información que deben recibir los hilos para poder ejecutar sus tareas
 * 
 */
typedef struct
{
    long n_hilo;    /*Indica qué numero de hilo es, desde el 0 al n-1, siendo n el número de hilos creados. El valor de n_hilo se utiliza para determinar el intervalo en el que el hilo debe buscar la solución*/
    long n_valores; /*Indica el número de valores que tendrá que buscar el hilo. Se multiplica por el número de hilos para encontrar los valores concretos que tiene que buscar*/
    long target;    /*Indica el valor cuya preimagen mediante la función hash se quiere encontrar*/
    int *p_finished;  /*Puntero al flag que emplearán los hilos para comunicarse entre sí si alguno de ellos termina*/
} ArgsSolucion;

/**
 * @brief Libera todos los recursos creados para la ejecución de hilos
 * 
 * @param n_threads indica el número de hilos creados
 * @param arg_array array de todas las estructuras creadas para ser pasadas como argumento a los hilos
 * @param thread_array array de los identificadores de los hilos
 */
void clean_and_free(int n_threads, ArgsSolucion **arg_array, pthread_t *thread_array)
{
    int k;
    for (k = 0; k < n_threads; k++)
    {
        free(arg_array[k]);
    }

    free(thread_array);
    free(arg_array);
}

/**
 * @brief aplica la función hash a todos los valores entre un intervalo dado
 * @param arg estructura en la que el hilo recibe toda la información que necesita para ejecutarse correctamente
 * @return int el valor que satisface la solución hash
 */
void *buscar_solucion(void *arg)
{
    long i = 0;
    long n_valores, n_hilo, target, *result;
    int *p_finished=NULL;

    /*Casteamos el array a su tipo correcto*/
    ArgsSolucion argssolucion;
    argssolucion = *((ArgsSolucion *)arg);

    /*Leemos los datos de los argumentos*/
    n_valores = argssolucion.n_valores;
    n_hilo = argssolucion.n_hilo;
    target = argssolucion.target;
    p_finished = argssolucion.p_finished;


    /*Reservamos memoria para la solución que devolveremos*/
    result = (long *)malloc(sizeof(long));

    /*Iteramos todos los valores posibles para encontrar el deseado*/
    for (i = n_valores * n_hilo; i < n_valores * (n_hilo + 1) && (*p_finished) == 0; i++)
    {
        if (pow_hash(i) == target)
        {
            *p_finished = 1;
            *result = i;
            pthread_exit(result);
        }
    }
    *result = SOLUTION_NOT_FOUND;
}

int main(int argc, char *argv[])
{
    long n_rounds = 0, target_ini = 0, pid = 0;
    int i = 0, j = 0, k = 0;
    long interval = 0;
    int n_threads = 0;
    int error;
    long solution;
    void *retval = NULL;
    int minero_escribe[2];
    int registrador_escribe[2];
    int fd;
    int finished = 0;

    char validado[] = "validated";
    char rejected[] = "rejected";
    char accepted[] = "accepted";
    char *status=NULL;

    char buffer[1024];

    pthread_t h, *thread_array = NULL;
    ArgsSolucion *a = NULL, **arg_array = NULL;

    /*Tratamiento de los argumentos de entrada*/
    if (argc != 4)
    {
        printf("Not enough arguments for the program\n");
        return EXIT_FAILURE;
    }
    target_ini = atoi(argv[1]);
    n_rounds = atoi(argv[2]);
    n_threads = atoi(argv[3]);

    if(target_ini < 0 || n_threads < 0){
        printf("Argumentos erroneos");
        exit(EXIT_FAILURE);
    }
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
    /*Proceso hijo: registrador*/
    if (pid == 0)
    {
        int round;
        char *pointer;


        sprintf(buffer, "%jd.log", (intmax_t)getppid());
        /*Se cierran los pipes que no necesitaremos y se abre el descriptor de fichero donde escribiremos los resultados*/

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
            status = strtok(NULL, "|\n\r");

            /*Escribe los resultados en el fichero*/
            dprintf(fd, "Id:%d \n"
                        "Winner:%jd \n"
                        "Target:%ld \n"
                        "Solution: %ld (%s)\n"
                        "Votes: %d/%d \n"
                        "Wallets: %jd:%d\n\n",
                    round, (intmax_t)getppid(), target_ini, solution, status, round, round, (intmax_t)getppid(), round);

            /**Manda señal de que ya ha escrito en el fichero */
            write(registrador_escribe[1], buffer, strlen(buffer) + 1);
        }
        close(minero_escribe[0]);
        close(registrador_escribe[1]);
        printf("Register exited with status 0\n");
        exit(EXIT_SUCCESS);
    }
    /*Proceso padre: minero*/
    else
    {
        /**Se cierran pipes pertinentes*/
        close(registrador_escribe[1]); /*registrador escribe (escritura)*/
        close(minero_escribe[0]);      /*minero escribe (lectura)*/

        /*Calculamos el intervalo de valores que recorrerá cada hilo*/
        interval = (POW_LIMIT - 1) / n_threads + 1;

        /*Reservamos la memoria necesaria para el programa*/
        arg_array = (ArgsSolucion **)malloc(sizeof(ArgsSolucion *) * n_threads);
        thread_array = (pthread_t *)malloc(sizeof(pthread_t) * n_threads);

        for (i = 0; i < n_threads; i++)
        {
            arg_array[i] = (ArgsSolucion *)malloc(sizeof(ArgsSolucion));
            arg_array[i]->n_valores = interval;
            arg_array[i]->n_hilo = i;
            arg_array[i]->p_finished = &finished;
        }

        /*Ejecutamos el código del minero*/
        for (i = 0; i < n_rounds; i++)
        {
            for (j = 0; j < n_threads; j++)
            {
                /*Preparar el argumento del hilo*/
                arg_array[j]->target = target_ini;

                /*Lanzamos el hilo*/
                error = pthread_create(&h, NULL, buscar_solucion, (void *)arg_array[j]);
                if (error != 0)
                {
                    fprintf(stderr, "pthread_create: %s\n", strerror(error));

                    /*Liberamos memoria y unimos todos los hilos antes de salir*/
                    for (k = 0; k < n_threads; k++)
                    {
                        free(arg_array[k]);
                        pthread_join(thread_array[k], &retval);
                    }
                    free(arg_array);
                    free(thread_array);

                    close(minero_escribe[1]);
                    wait(NULL);
                    printf("Miner exited with status 1\n");
                    exit(EXIT_FAILURE);
                }
                thread_array[j] = h;
            }

            /*Comprobacion de resultados y join de los hilos*/
            for (j = 0; j < n_threads; j++)
            {
                pthread_join(thread_array[j], &retval);
                if (*(long *)retval != SOLUTION_NOT_FOUND)
                {
                    /*Nos guardamos la solución que sea correcta, pues los hilos que terminan sin encontrar una solución devuelven -1*/
                    solution =  *(long *)retval;
                }
                free(retval);
            }

            /*Determinamos si la solución será validada o rechazada. En esta iteración, se toma como criterio arbitrario que la solución sea múltiplo de 10*/
            status = VALIDATE(solution) ? validado : rejected;

            /*Se ha encontrado una solución, se le envía al registrador*/
            sprintf(buffer, "%d|%ld|%ld|%s\n", i, target_ini, solution, status);
            write(minero_escribe[1], buffer, strlen(buffer) + 1);

            /*Imprimimos por terminal también*/
            printf("Solution %s: %ld------->%ld\n", VALIDATE(solution)?accepted:rejected, target_ini, solution);

            target_ini = solution;

            /**Leemos la señal del registrador para continuar con la siguiente ronda */
            if (read(registrador_escribe[0], buffer, sizeof(buffer)) <= 0)
            {
                clean_and_free(n_threads, arg_array, thread_array);
                printf("Miner exited with status 0\n");
                wait(NULL);
                return EXIT_SUCCESS;
            }
            finished = 0;
        }

        /**Numero de rondas terminado, mandamos señal de final y liberamos memoria */
        close(minero_escribe[1]);
        clean_and_free(n_threads, arg_array, thread_array);
        wait(NULL);
        printf("Miner exited with status 0\n");
    }
    return EXIT_SUCCESS;
}