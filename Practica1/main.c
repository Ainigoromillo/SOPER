/**
 * @file main.c
 * @author Alvaro Iñigo y Matteo Artuñedo
 * @brief implementa un programa en el que varios mineros (hilos) tendrán que
 * buscar la preimagen de un valor por una función hash y comunicarse con un
 * registrador para escribirlo.
 * @version 0.2
 * @date 2026-02-28
 *
 */
/* Expose POSIX APIs such as sigprocmask and SIG_BLOCK */
#define _POSIX_C_SOURCE 200809L

#include "pow.h"
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERR -1
#define MAX_BUFFER 20
#define MUTEX_PIDS_SEM_NAME "/mutex_pids_sem"
#define MUTEX_TARGET_SEM_NAME "/mutex_target"
#define GANADOR_SEM "/ganador_sem"

#define FICHERO_SISTEMA                                                        \
  "Pids.pid" /**Nombre del fichero donde se guardarán los pids de los mineros \
                que participen en las carreras*/
#define FICHERO_SISTEMA_TEMP                                                        \
  "Pids__temp.pid" /**Nombre del fichero donde se guardarán los pids de los mineros \
                temporalmente en el proceso de borrar uno de ellos*/
#define FICHERO_TARGET                                                         \
  "Target.tgt" /**Nombre del fichero donde se esciribrá el target que usarán \
                  los mineros que participen en las carreras*/
#define FICHERO_TARGET_TEMP                                                         \
  "Target.tgt" /**Nombre del fichero temporal donde se esciribrá el target que usarán \
                  los mineros que participen en las carreras*/

#define SOLUTION_NOT_FOUND                                                     \
  -1 /**Valor que devuelven los hilos si no han encontrado una solución para  \
        el target */

/**En esta iteración, determinaremos que una solución será rechazada si es
 * múltiplo de diez. Este criterio será modificado en iteraciones futuras */
#define VALIDATE(solution) ((solution) % 10 == 0 ? (0) : (1))

/**Variable que indica si el minero ha recibido la señal que indica que ha
 * terminado su tiempo*/
int terminar = 0;

/**Función responsable de gestionar la llegada de señales SIGALRM*/
void handler_alarm(int sig) { terminar = 1; }

/**Función responsable de gestionar la llegada de señales SIGUSR1*/
void handler_sigusr1(int sig) { }

/**Función responsable de gestionar la llegada de señales SIGUSR2*/
void handler_sigusr2(int sig) { }

/**
 * @brief Estructura que almacena la información que deben recibir los hilos
 * para poder ejecutar sus tareas
 *
 */
typedef struct {
  long n_hilo; /*Indica qué numero de hilo es, desde el 0 al n-1, siendo n el
                  número de hilos creados. El valor de n_hilo se utiliza para
                  determinar el intervalo en el que el hilo debe buscar la
                  solución*/
  long n_valores; /*Indica el número de valores que tendrá que buscar el hilo.
                     Se multiplica por el número de hilos para encontrar los
                     valores concretos que tiene que buscar*/
  long target;    /*Indica el valor cuya preimagen mediante la función hash se
                     quiere encontrar*/
  int *p_finished; /*Puntero al flag que emplearán los hilos para comunicarse
                      entre sí si alguno de ellos termina*/
} ArgsSolucion;

/**
 * @brief Funcion con la cual el minero que lidere iniciará la carrera
 */
void start_race(int caller_pid){
  FILE *f=NULL;
  char buffer[MAX_BUFFER];
  int pid;
  if (!(f = fopen(FICHERO_SISTEMA, "r"))) {
      perror("fopen fichero sistema");
      exit(EXIT_FAILURE);;
  }
  while(fgets(buffer, MAX_BUFFER, f)){
    pid = atoi(buffer);
    if(pid != caller_pid) kill((pid_t)pid, SIGUSR1);
  }
  fclose(f);

}

/**
 * @brief Funcion con la cual el minero que lidere iniciará la votacion
 */
void start_votation(int caller_pid){
  FILE *f=NULL;
  char buffer[MAX_BUFFER];
  int pid;
  if (!(f = fopen(FICHERO_SISTEMA, "r"))) {
      perror("fopen fichero sistema");
      exit(EXIT_FAILURE);;
  }
  while(fgets(buffer, MAX_BUFFER, f)){
    pid = atoi(buffer);
    if(pid != caller_pid) kill((pid_t)pid, SIGUSR2);
  }
  fclose(f);

}

/**
 * @brief Funcion con la cual los mineros se apuntan a la lista de mineros activos
 * @param pid identificador del minero que se inscribe 
 */
void inscribirseLista(int pid){
  FILE *f=NULL;
  char buffer[MAX_BUFFER];
  if (!(f = fopen(FICHERO_SISTEMA, "a"))) {
      perror("fopen fichero sistema");
      exit(EXIT_FAILURE);
  }
  /*Nos apuntamos en la lista de pid's*/
  fprintf(f, "%d\n", pid);
  fclose(f);

}
/**
 * @brief Funcion con la cual los mineros se desapuntan de la lista de mineros activos
 * @param pid identificador del minero que se inscribe 
 */
void desinscribirseLista(int pid){
  FILE *f=NULL;
  FILE *f2=NULL;
  int read_pid = 0;
  char buffer[MAX_BUFFER];
  if (!(f = fopen(FICHERO_SISTEMA, "r"))) {
      perror("fopen fichero sistema");
      exit(EXIT_FAILURE);
  }
  if (!(f2 = fopen(FICHERO_SISTEMA_TEMP, "w"))) {
      perror("fopen fichero sistema");
      exit(EXIT_FAILURE);
  }
  /*Vamos copiando las lineas una a una salvo aquella que queremos eliminar*/
  while(fgets(buffer, MAX_BUFFER, f)){
    read_pid = atoi(buffer);
    if(read_pid != pid){
      fprintf(f2, "%d\n", read_pid);  
    }
  }
  fclose(f);
  fclose(f2);
  /*Borramos el archivo original y al archivo temporal le damos el nomre del original*/
  remove(FICHERO_SISTEMA);
  rename(FICHERO_SISTEMA_TEMP, FICHERO_SISTEMA);
  //unlink(FICHERO_SISTEMA);
}

/**
 * @brief Funcion con la cual el minero ganador reestablecerá el target del resto de mineros
 */
void new_target(int target){
  FILE *f=NULL;
  char buffer[MAX_BUFFER];
  if (!(f = fopen(FICHERO_TARGET_TEMP, "w+"))) {
      perror("fopen fichero sistema");
      exit(EXIT_FAILURE);
  }
  fprintf(f, "%d\n", target);  
  fclose(f);
  /*Borramos el archivo original y al archivo temporal le damos el nomre del original*/
  remove(FICHERO_TARGET);
  rename(FICHERO_TARGET_TEMP, FICHERO_TARGET);
}

/**
 * @brief Funcion con la cual los mineros podrán leer el target actual
 */
int leer_target(){
  FILE *f=NULL;
  int ret = 0;
  char buffer[MAX_BUFFER];
  if (!(f = fopen(FICHERO_TARGET_TEMP, "r"))) {
    return ERR;
  }
  /*Vamos copiando las lineas una a una salvo aquella que queremos eliminar*/
  fgets(buffer, MAX_BUFFER, f);
 
  ret = atoi(buffer);
  fclose(f);
  return ret;
}

/**
 * @brief Libera todos los recursos creados para la ejecución de hilos
 *
 * @param n_threads indica el número de hilos creados
 * @param arg_array array de todas las estructuras creadas para ser pasadas como
 * argumento a los hilos
 * @param thread_array array de los identificadores de los hilos
 */
void clean_and_free(int n_threads, ArgsSolucion **arg_array,
                    pthread_t *thread_array) {
  int k;
  for (k = 0; k < n_threads && arg_array; k++) {
    if ((arg_array)[k])
      free(arg_array[k]);
  }

  if (thread_array)
    free(thread_array);
  if (arg_array)
    free(arg_array);
}

/**
 * @brief aplica la función hash a todos los valores entre un intervalo dado
 * @param arg estructura en la que el hilo recibe toda la información que
 * necesita para ejecutarse correctamente
 * @return int el valor que satisface la solución hash
 */
void *buscar_solucion(void *arg) {
  long i = 0;
  long n_valores, n_hilo, target, *result;
  int *p_finished = NULL;

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
  for (i = n_valores * n_hilo;
       i < n_valores * (n_hilo + 1) && (*p_finished) == 0; i++) {
    if (pow_hash(i) == target) {
      *p_finished = 1;
      *result = i;
      pthread_exit(result);
    }
  }
  *result = SOLUTION_NOT_FOUND;
}

int main(int argc, char *argv[]) {
  long pid = 0;
  int i = 0, j = 0, k = 0;
  long interval = 0;
  int n_secs = 0;
  int n_threads = 0;
  int error;
  long solution;
  void *retval = NULL;
  int ret_int;
  int minero_escribe[2];
  int registrador_escribe[2];
  int fd;
  int finished = 0;
  sem_t *mutex_pids = NULL;
  sem_t *mutex_target = NULL;
  sem_t *ganador_sem = NULL;
  sigset_t origMask, block2mask, block1mask;
  int n_miners;
  int ganador = 0;
  int target;

  char validado[] = "validated";
  char rejected[] = "rejected";
  char accepted[] = "accepted";
  char *status = NULL;

  char buffer[1024];
  struct sigaction act_alarm, act_sigusr1, act_sigusr2;

  pthread_t h, *thread_array = NULL;
  ArgsSolucion *a = NULL, **arg_array = NULL;

  /*Descomentar este código solo si se sospecha que pueda haber algún semáforo en el sistema que no se haya borrado correctamente*/
  // sem_unlink(GANADOR_SEM);
  // sem_unlink(MUTEX_PIDS_SEM_NAME);
  // sem_unlink(MUTEX_TARGET_SEM_NAME);

  /*Tratamiento de los argumentos de entrada*/
  if (argc != 3) {
    printf("Not enough arguments for the program\n");
    return EXIT_FAILURE;
  }
  n_secs = atoi(argv[1]);
  n_threads = atoi(argv[2]);

  if (n_threads <= 0 | n_secs < 0) {
    printf("Argumentos erróneos");
    exit(EXIT_FAILURE);
  }
  /*Apertura del pipe*/
  pipe(minero_escribe);
  pipe(registrador_escribe);

  /*Aplicamos la máscara que será empleada por los mineros*/
  /*Bloqueamos la señal 1, pues será la que podrán recibir los mineros que no lideren las carreras*/
  sigemptyset(&block1mask);
  sigaddset(&block1mask, SIGUSR1);
  if (sigprocmask(SIG_BLOCK, &block1mask, &origMask) == -1){
    perror("sigprocmask");
  }
  /*Preparamos la máscara que aplicaremos una vez comience la carrera para que los mineros perdedores puedan votar*/
  sigemptyset(&block2mask);
  sigaddset(&block2mask, SIGUSR2);

  

  /*División de los procesos*/
  pid = fork();
  if (pid < 0) {
    close(minero_escribe[0]);
    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    close(registrador_escribe[1]);
    perror("Error en el fork\n");
    return EXIT_FAILURE;
  }
  /*Proceso hijo: registrador*/
  if (pid == 0) {
    // int round;
    // char *pointer;
    //
    // sprintf(buffer, "%jd.log", (intmax_t)getppid());
    //
    // /*Se cierran los pipes que no necesitaremos y se abre el descriptor de
    //  * fichero donde escribiremos los resultados*/
    // close(minero_escribe[1]);      /*minero escribe (write) */
    // close(registrador_escribe[0]); /*registrador escribe (read) */
    //
    // if ((fd = open(buffer, O_CREAT | O_TRUNC | O_RDWR,
    //                S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1) {
    //   perror("open");
    //   close(minero_escribe[0]);
    //   close(registrador_escribe[1]);
    //
    //   printf("Register exited with status 1\n");
    //   exit(EXIT_FAILURE);
    // }
    //
    // while (read(minero_escribe[0], buffer, sizeof(buffer)) > 0) {
    //
    //   pointer = strtok(buffer, "|\n\r");
    //   round = atoi(pointer) + 1;
    //   pointer = strtok(NULL, "|\n\r");
    //   solution = atol(pointer);
    //   status = strtok(NULL, "|\n\r");
    //
    //   /*Escribe los resultados en el fichero*/
    //   dprintf(fd,
    //           "Id:%d \n"
    //           "Winner:%jd \n"
    //           "Target:%ld \n"
    //           "Solution: %ld (%s)\n"
    //           "Votes: %d/%d \n"
    //           "Wallets: %jd:%d\n\n",
    //           round, (intmax_t)getppid(), solution, status, round, round,
    //           (intmax_t)getppid(), round);
    //
    //   /**Manda señal de que ya ha escrito en el fichero */
    //   write(registrador_escribe[1], buffer, strlen(buffer) + 1);
    // }
    // close(minero_escribe[0]);
    // close(registrador_escribe[1]);
    // close(fd);
    // printf("Register exited with status 0\n");
    // exit(EXIT_SUCCESS);
  }
  /*Proceso padre: minero*/
  else {
    // Configuramos la señal de alarma
    act_alarm.sa_handler = handler_alarm;
    sigemptyset(&(act_alarm.sa_mask));
    act_alarm.sa_flags = 0;

    if (sigaction(SIGALRM, &act_alarm, NULL) < 0) {
      perror(" sigaction ");
      exit(EXIT_FAILURE);
    }
    // Iniciamos la cuenta con la alarma
    alarm(n_secs);
    
    // Configuramos el manejador para sigursr1
    act_sigusr1.sa_handler = handler_sigusr1;
    sigemptyset(&(act_sigusr1.sa_mask));
    act_sigusr1.sa_flags = 0;

    if (sigaction(SIGUSR1, &act_sigusr1, NULL) < 0) {
      perror(" sigaction ");
      exit(EXIT_FAILURE);
    }

    // Configuramos el manejador para sigursr2
    act_sigusr2.sa_handler = handler_sigusr2;
    sigemptyset(&(act_sigusr2.sa_mask));
    act_sigusr2.sa_flags = 0;

    if (sigaction(SIGUSR1, &act_sigusr2, NULL) < 0) {
      perror(" sigaction ");
      exit(EXIT_FAILURE);
    }

    /**Se cierran pipes pertinentes*/
    // close(registrador_escribe[1]); /*registrador escribe (escritura)*/
    // close(minero_escribe[0]);      /*minero escribe (lectura)*/

    /*Accedemos al fichero de los pids*/
    if ((mutex_pids = sem_open(MUTEX_PIDS_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
        SEM_FAILED) {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }
    sem_wait(mutex_pids); /*Accedemos a sección crítica: el fichero de pid's*/
    inscribirseLista(getpid());
    sem_post(mutex_pids);

    if ((ganador_sem = sem_open(GANADOR_SEM, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
        SEM_FAILED) {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }

    /*Calculamos el intervalo de valores que recorrerá cada hilo*/
    interval = (POW_LIMIT - 1) / n_threads + 1;

    /*Reservamos la memoria necesaria para el programa*/
    arg_array = (ArgsSolucion **)malloc(sizeof(ArgsSolucion *) * n_threads);
    thread_array = (pthread_t *)malloc(sizeof(pthread_t) * n_threads);

    if (!arg_array || !thread_array) {
      if (arg_array)
        free(arg_array);
      if (thread_array)
        free(thread_array);
      close(minero_escribe[1]);
      close(registrador_escribe[0]);
      wait(NULL);
      exit(EXIT_FAILURE);
    }

    for (i = 0; i < n_threads; i++) {
      arg_array[i] = (ArgsSolucion *)malloc(sizeof(ArgsSolucion));
      if (!arg_array[i]) {
        close(minero_escribe[1]);
        close(registrador_escribe[0]);
        clean_and_free(n_threads, arg_array, thread_array);
        wait(NULL);
        exit(EXIT_FAILURE);
      }
      arg_array[i]->n_valores = interval;
      arg_array[i]->n_hilo = i;
      arg_array[i]->p_finished = &finished;
    }

    /*Accedemos al fichero de los targets*/
    if ((mutex_target = sem_open(MUTEX_TARGET_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
        SEM_FAILED) {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }
    sem_wait(mutex_target); 
    target = leer_target(); 
    /*Si no hay un target, lo añadimos nosotros y somos nosotros los que iniciamos la primera carrera*/
    if(target == ERR){
      new_target(0);
      target = 0;
      ganador = 1;
      /**Como este proceso es el primero, no recibirá SIGUSR1 sino que directamente podrá recibir SIGUSR2 para la votación*/
      if (sigprocmask(SIG_BLOCK, &block2mask, &block1mask) == -1){
        perror("sigsuspend");
      }  
    }
    sem_post(mutex_target);


    /*Ejecutamos el código del minero*/
    while (terminar == 0) {
      if(ganador == 1){
        start_race(getpid());
      }else{
        /*Esperamos a recibir la señal de comienzo de la carrera*/
        /*Aplicamos la máscara que bloqueará la señal dos, que es la que los mineros que pierdan aplicarán posteriormente*/
        /*Le pasamos así una máscara en la que no aparece SIGUSR1 para que el prceso se desbloquee con dicha señal*/
        if (sigsuspend(&block2mask) == -1){
          perror("sigsuspend");
        }  
      }
      for (j = 0; j < n_threads; j++) {
        /*Preparar el argumento del hilo*/
        arg_array[j]->target = target;

        /*Lanzamos el hilo*/
        error = pthread_create(&h, NULL, buscar_solucion, (void *)arg_array[j]);
        if (error != 0) {
          fprintf(stderr, "pthread_create: %s\n", strerror(error));

          /*Liberamos memoria y unimos todos los hilos antes de salir*/
          for (k = 0; k < n_threads; k++) {
            free(arg_array[k]);
            pthread_join(thread_array[k], &retval);
          }
          free(arg_array);
          free(thread_array);

          close(minero_escribe[1]);
          close(registrador_escribe[0]);
          wait(NULL);
          printf("Miner exited with status 1\n");
          exit(EXIT_FAILURE);
        }
        thread_array[j] = h;
      }

      /*Comprobacion de resultados y join de los hilos*/
      for (j = 0; j < n_threads; j++) {
        pthread_join(thread_array[j], &retval);
        if (*(long *)retval != SOLUTION_NOT_FOUND) {
          /*Nos guardamos la solución que sea correcta, pues los hilos que
           * terminan sin encontrar una solución devuelven -1*/
          solution = *(long *)retval;
        }
        free(retval);
      }
      
      /*El proceso intenta ser el ganador, quedando bloqueado en caso contrario*/
      ret_int = sem_trywait(ganador_sem);
      if(ret_int == 0){
        /*El proceso es el ganador*/
        printf("Soy el ganador!");
        new_target(solution);
        start_votation(getpid());
        /*Esperamos a que todos los procesos voten*/
        sem_post(ganador_sem);
      }
      else{
        perror("sem_trywait");
        printf("Tengo que esperar al ganador!");
        ganador = 0;
        if (sigsuspend(&block1mask) == -1){
          perror("sigsuspend");
        }
      }


    //   /*Determinamos si la solución será validada o rechazada. En esta
    //    * iteración, se toma como criterio arbitrario que la solución sea
    //    * múltiplo de 10*/
    //   status = VALIDATE(solution) ? validado : rejected;
    //
    //   /*Se ha encontrado una solución, se le envía al registrador*/
    //   // sprintf(buffer, "%d|%ld|%ld|%s\n", i, target_ini, solution, status);
    //   write(minero_escribe[1], buffer, strlen(buffer) + 1);
    //
    //   /*Imprimimos por terminal también*/
    //   // printf("Solution %s: %ld------->%ld\n",
    //   // VALIDATE(solution)?accepted:rejected, target_ini, solution);
    //
    //   // target_ini = solution;
    //
    //   /**Leemos la señal del registrador para continuar con la siguiente ronda
    //    */
    //   if (read(registrador_escribe[0], buffer, sizeof(buffer)) <= 0) {
    //     close(minero_escribe[1]);
    //     close(registrador_escribe[0]);
    //     clean_and_free(n_threads, arg_array, thread_array);
    //     printf("Miner exited with status 0\n");
    //     wait(NULL);
    //     return EXIT_SUCCESS;
    //   }
    //   finished = 0;
    // }
    }
    /**Numero de rondas terminado, nos desapuntamos de la lista, mandamos señal de final y liberamos memoria*/
    sem_wait(mutex_pids); /*Accedemos a sección crítica: el fichero de pid's*/
    desinscribirseLista(getpid());
    sem_post(mutex_pids);
    sem_close(mutex_pids);
    sem_close(mutex_target);
    sem_close(ganador_sem);

    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    // clean_and_free(n_threads, arg_array, thread_array);
    wait(NULL);
    sem_unlink(MUTEX_PIDS_SEM_NAME);
    sem_unlink(MUTEX_TARGET_SEM_NAME);
    sem_unlink(GANADOR_SEM);
    printf("Miner exited with status 0\n");
    return EXIT_SUCCESS;
  }
}
