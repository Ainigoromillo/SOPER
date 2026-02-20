#include<stdio.h>
#include <pthread.h>
#include<unistd.h>
#include<sys/wait.h>
#include<stdlib.h>
#include<string.h>
#include "pow.h"

int finished = 0;

typedef struct{
    long n_hilo;
    long n_valores;
    long target;
} ArgsSolucion;

/**
 * @brief aplica la función hash a todos los valores entre un intervalo dado
 * 
 * @return int el valor que satisface la solución hash
 */
void * buscar_solucion(void *arg){
    long i=0;
    long n_valores, n_hilo, target, *result;

    ArgsSolucion argssolucion;
    argssolucion = *((ArgsSolucion *)arg);

    result = (long *)malloc(sizeof(long));

    n_valores = argssolucion.n_valores;
    n_hilo = argssolucion.n_hilo;
    target = argssolucion.target;

    
    for(i=n_valores * n_hilo;i<n_valores * (n_hilo+1) && finished == 0;i++){
        if(pow_hash(i) == target){
            finished = 1;
            *result = i;
            pthread_exit(result);
        }
    }
    *result = -1;
}

int main(int argc, char *argv[]){
    long n_rounds=0,target_ini=0, pid=0,i=0, j=0;
    long interval=0;
    int n_threads = 0;
    int error;
    void *retval=NULL;
    pthread_t h, *thread_array=NULL;
    ArgsSolucion *a=NULL, **arg_array=NULL;

    /*Tratamiento de los argumentos de entrada*/
    if(argc != 4){
        printf("Not enough arguments for the program\n");
        return EXIT_FAILURE;
    }
    target_ini = atoi(argv[1]);
    n_rounds = atoi(argv[2]);
    n_threads = atoi(argv[3]);


    /*División de los procesos*/
   pid = fork();
   if(pid < 0){
    perror("Error en el fork\n");
    return EXIT_FAILURE;
   } 
   if(pid==0){
    printf("Soy el registrador\n");
    exit(EXIT_SUCCESS);
   }
   else{
    /*Calculamos el intervalo de valores que recorrerá cada hilo*/
    interval = (POW_LIMIT - 1)/n_threads + 1;

    /*Reservamos la memoria necesaria para el programa*/
    arg_array = (ArgsSolucion **)malloc(sizeof(ArgsSolucion *) * n_threads);
    thread_array = (pthread_t *)malloc(sizeof(pthread_t) * n_threads);

    /*Ejecutamos el código del minero*/
    for(i=0;i<n_rounds;i++){
        for(j=0;j<n_threads;j++){
            /*Preparar el argumento del hilo*/
            a = (ArgsSolucion *)malloc(sizeof(ArgsSolucion));
            a->n_hilo = j;
            a->n_valores = interval;
            a->target = target_ini;
            arg_array[j] = a;

            /*Lanzamos el hilo*/
            error = pthread_create(&h, NULL, buscar_solucion, (void *)a);
            if (error != 0) {
                fprintf(stderr, "pthread_create: %s\n", strerror(error));
                /*Falta liberar más memoria de otras cosas*/
                free(a);
                exit(EXIT_FAILURE);
            }
            thread_array[j] = h; /*Asegurar que esto funcione bien y que realmente se guarde el valor actualizado*/

        }
        for(j=0;j<n_threads;j++){
            pthread_join(thread_array[j], &retval);
            if(*(long *)retval != -1){
                target_ini = *(long *)retval;
                printf("La preimagen es %ld\n", target_ini);
            }
            free(retval);
            free(arg_array[j]);
        } 
        printf("\n%d, ", i);
        printf("\nTarget es: %ld", target_ini);
    }
    free(thread_array);
    free(arg_array);
    printf("Soy el minero\n");
    wait(NULL);
   }

   return EXIT_SUCCESS;
}