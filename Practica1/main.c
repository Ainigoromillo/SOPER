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
#include <errno.h>
#define _POSIX_C_SOURCE 200809L

#include "pow.h"
#include <time.h>
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
#include <stdatomic.h>
#include <sys/mman.h>

atomic_int finished = 0; // Variable global compartida por los hilos que indica que deben dejar de minar
#define NO_TARGET -1
#define MAX_BUFFER 4
#define MAX_INTENTOS 500 // El numero maximo de esperas que hace el proceso ganador a que los demas voten
#define YES 'y'
#define NO 'n'
#define MUTEX_PIDS_SEM_NAME "/mutex_pids_sem"
#define MUTEX_TARGET_SEM_NAME "/mutex_target"
#define GANADOR_SEM "/ganador_sem"
#define MUTEX_VOTACION_SEM_NAME "/mutex_voting"
#define MAX_MINEROS 20


#define FICHERO_SISTEMA                                                        \
  "Pids.pid" /**Nombre del fichero donde se guardarán los pids de los mineros \
                que participen en las carreras*/
#define FICHERO_SISTEMA_TEMP                                                        \
  "Pids_temp.pid" /**Nombre del fichero donde se guardarán los pids de los mineros \
                temporalmente en el proceso de borrar uno de ellos*/
#define FICHERO_TARGET                                                         \
  "Target.tgt" /**Nombre del fichero donde se esciribrá el target que usarán \
                  los mineros que participen en las carreras*/

#define FICHERO_VOTACION                                                      \
  "Voting.vot" /**Nombre del fichero donde los procesos perdedores apuntan su \
                  votacion y el proceso ganador comprueba si todos han votado */

#define SOLUTION_NOT_FOUND                                                    \
  -1 /**Valor que devuelven los hilos si no han encontrado una solución para \
        el target */

// Condicion de solucion correcta, por ahora tiene una probabilidad de 75% de validarse
#define VALIDATE(solution) (rand() % 4 != 0)

/**Variable que indica si el minero ha recibido la señal que indica que ha
 * terminado su tiempo*/
atomic_int terminar = 0;

/**Función responsable de gestionar la llegada de señales SIGALRM*/
void handler_alarm(int sig)
{
  printf("alarma <%d>\n", getpid());
  terminar = 1;
}

/**Función responsable de gestionar la llegada de señales SIGUSR1*/
void handler_sigusr1(int sig)
{
}

/**Función responsable de gestionar la llegada de señales SIGUSR2*/
void handler_sigusr2(int sig)
{
  // simplemente esta señal indica que los procesos deben dejar de minar
  finished = 1;
}

/**
 * @brief Estructura que almacena la información que deben recibir los hilos
 * para poder ejecutar sus tareas
 *
 */
typedef struct
{
  long n_hilo;    /*Indica qué numero de hilo es, desde el 0 al n-1, siendo n el
                     número de hilos creados. El valor de n_hilo se utiliza para
                     determinar el intervalo en el que el hilo debe buscar la
                     solución*/
  long n_valores; /*Indica el número de valores que tendrá que buscar el hilo.
                     Se multiplica por el número de hilos para encontrar los
                     valores concretos que tiene que buscar*/
  long target;    /*Indica el valor cuya preimagen mediante la función hash se
                     quiere encontrar*/
} ArgsSolucion;

/**
 * @brief Funcion con la cual el minero que lidere iniciará la carrera
 * @param el pid del proceso que llama a la función
 */
void start_race(int caller_pid)
{
  int fd_shm;
  int *pids=NULL;
  int i;

  if (( fd_shm = shm_open ( FICHERO_SISTEMA , O_RDONLY, 0) ) == -1) {
    perror ( " shm_open start_race " ) ;
    exit ( EXIT_FAILURE ) ;
  }
   
  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ, MAP_PRIVATE, fd_shm, 0);
  if(pids == MAP_FAILED){
    close(fd_shm);
    perror("mmap en start_race");
    exit(EXIT_FAILURE);
  }
  for(i=0;i<MAX_MINEROS;i++){
    if(pids[i] == 0) break;
    if(pids[i] != caller_pid){
      kill((pid_t)pids[i], SIGUSR1);
    } 
  } 
  close(fd_shm);
  munmap(pids, MAX_MINEROS * sizeof(int));
}

/**
 * @brief Funcion con la cual el minero que lidere iniciará la votacion
 * @param el pid del proceso que llama a la función
 */
void start_votation(int caller_pid)
{
  int fd_shm;
  int *pids=NULL;
  int i;

  fd_shm = shm_open ( FICHERO_VOTACION , O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR ) ;
  if ( !(fd_shm == -1) ) {
    if((ftruncate(fd_shm, sizeof(char) * MAX_MINEROS)) == -1){
      perror("ftruncate");
      sem_unlink(FICHERO_VOTACION);
      exit(EXIT_FAILURE);
    }
  } 
  close(fd_shm);

  if (( fd_shm = shm_open ( FICHERO_SISTEMA , O_RDONLY, 0) ) == -1) {
    perror ( " shm_open start_votation" ) ;
    exit ( EXIT_FAILURE ) ;
  }
   
  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ, MAP_PRIVATE, fd_shm, 0);
  if(pids == MAP_FAILED){
    close(fd_shm);
    perror("mmap en start_votation");
    exit(EXIT_FAILURE);
  }

  for(i=0;i<MAX_MINEROS;i++){
    if(pids[i] == 0) break;
    if(pids[i] != caller_pid){
      kill((pid_t)pids[i], SIGUSR2);
    } 
  } 
  close(fd_shm);
  munmap(pids, MAX_MINEROS * sizeof(int));
}

/**
 * @brief Funcion con la cual los mineros se apuntan a la lista de mineros activos
 * @param pid identificador del minero que se inscribe
 */
void inscribirseLista(int pid)
{
int fd_shm;
  int *pids=NULL;
  int i;

  fd_shm = shm_open ( FICHERO_SISTEMA , O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR ) ;
  if ( fd_shm == -1) {
    if ( errno == EEXIST ) {
      fd_shm = shm_open ( FICHERO_SISTEMA , O_RDWR , 0) ;
      if ( fd_shm == -1) {
        perror ( " Error opening the shared memory segment \n " ) ;
        close(fd_shm);
        exit ( EXIT_FAILURE ) ;
      } 
    } else {
      perror ( " Error creating the shared memory segment \n " ) ;
      close(fd_shm);
      exit ( EXIT_FAILURE ) ;
    }
  }else{
    if((ftruncate(fd_shm, sizeof(int) * MAX_MINEROS)) == -1){
      perror("ftruncate");
      sem_unlink(FICHERO_SISTEMA);
      exit(EXIT_FAILURE);
    }else{
    }
  }
   
  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if(pids == MAP_FAILED){
    close(fd_shm);
    perror("mmap en inscribirseLista");
    exit(EXIT_FAILURE);
  }
  if(pids == MAP_FAILED){
    close(fd_shm);
    perror("mmap en inscribirseLista");
    exit(EXIT_FAILURE);
  }
  for(i=0;i<MAX_MINEROS;i++){
    if(pids[i] != 0){
      printf("<%d>\n", pids[i]);
    }else{
      pids[i] = pid; 
      printf("miner <%d> added to the system\n", pid);
      break;
    }
  } 
  close(fd_shm);
  munmap(pids, MAX_MINEROS * sizeof(int));
}

/**
 * @brief Funcion con la que los procesos votan si la solucion es correcta o no
 *
 * @param solution la solucion obtenida por el proceso ganador
 */
void votar(int solution)
{
  int fd_shm;
  int i;
  char *votations = NULL;

  fd_shm = shm_open ( FICHERO_VOTACION , O_RDWR | O_EXCL , S_IRUSR | S_IWUSR ) ;
  if ( fd_shm == -1) {
    if ( errno == EEXIST ) {
      fd_shm = shm_open ( FICHERO_VOTACION , O_RDWR , 0) ;
      if ( fd_shm == -1) {
        perror ( " Error opening the shared memory segment \n " ) ;
      close(fd_shm);
        exit ( EXIT_FAILURE ) ;
      } else {
      }
    } else {
      perror ( " Error creating the shared memory segment \n " ) ;
      close(fd_shm);
      exit ( EXIT_FAILURE ) ;
    }
 }
   
  votations = mmap(NULL, MAX_MINEROS * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if(votations == MAP_FAILED){
    close(fd_shm);
    perror("mmap en votar");
    exit(EXIT_FAILURE);
  }

  for(i=0;i<MAX_MINEROS;i++){
    if(votations[i] == 0){
      votations[i] = VALIDATE(solution) ? YES : NO;
      break;
    }
  }
  close(fd_shm);
  munmap(votations, MAX_MINEROS * sizeof(char));
}

/**
 * @brief Funcion con la cual los mineros se desapuntan de la lista de mineros activos
 * @param pid identificador del minero que se inscribe
 */
void desinscribirseLista(int pid)
{
  int fd_shm;
  int *pids=NULL;
  int i;

  if (( fd_shm = shm_open ( FICHERO_SISTEMA , O_RDWR, 0 ) ) == -1) {
    perror ( " shm_open en desinscribirseLista" ) ;
    exit ( EXIT_FAILURE ) ;
  }
   
  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if(pids == MAP_FAILED){
    close(fd_shm);
    perror("mmap en desinscribirseLista");
    exit(EXIT_FAILURE);
  }

  for(i=0;i<MAX_MINEROS;i++){
    if(pids[i] == pid){
      pids[i] = 0;
      i++;
      break;
    }
  } 
  //Una vez hemos encontrado la posición en la que estaba nuestro pid, desplazamos el resto
  for(;i>MAX_MINEROS;i++){
    pids[i-1] = pids[i];
  }
  close(fd_shm);
  munmap(pids, MAX_MINEROS * sizeof(int));
  printf("miner <%d> exited the system\n", pid);
}

/**
 * @brief Funcion con la cual el minero ganador reestablecerá el target del resto de mineros
 * @param el nuevo targert
 */
void new_target(int target)
{
  int fd_shm;
  int *pids=NULL;
  int *shared_target = NULL;

  fd_shm = shm_open ( FICHERO_TARGET , O_RDWR , 0) ;
  if ( fd_shm == -1) {
      perror ( " Error creating the shared memory segment \n " ) ;
      exit ( EXIT_FAILURE ) ;
  }
   
  shared_target = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if(pids == MAP_FAILED){
    close(fd_shm);
    perror("mmap en new_target");
    exit(EXIT_FAILURE);
  }

  shared_target[0] = target;
   
  close(fd_shm);
  munmap(shared_target, sizeof(int));
}

/**
 * @brief Funcion con la cual los mineros podrán leer el target actual
 */
int leer_target()
{
  int *target = NULL;
  int fd_shm;
  int ret = 0;
  int ganador = 0;

  fd_shm = shm_open ( FICHERO_TARGET , O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR ) ;
  if ( fd_shm == -1) {
    if ( errno == EEXIST ) {
      fd_shm = shm_open ( FICHERO_TARGET , O_RDWR , 0) ;
      if ( fd_shm == -1) {
        perror ( " Error opening the shared memory segment \n " ) ;
        close(fd_shm);
        exit ( EXIT_FAILURE ) ;
      } 
    } else {
      perror ( " Error creating the shared memory segment \n " ) ;
      close(fd_shm);
      exit ( EXIT_FAILURE ) ;
    }
  }else{
    ganador = 1;
    if((ftruncate(fd_shm, sizeof(int)) * 1) == -1){
      perror("ftruncate leer_target");
      sem_unlink(FICHERO_TARGET);
      exit(EXIT_FAILURE);
    }
    
  }
  target = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if(target == MAP_FAILED){
    close(fd_shm);
    perror("mmap en leer_target");
    exit(EXIT_FAILURE);
  }
  ret = *target;
  
  close(fd_shm);
  munmap(target, sizeof(int));
  if(ganador == 1) return NO_TARGET;
  return ret;
    
  }

/**
 * @brief Devuelve el numero de corredores apuntados en el sistema
 *
 * @return int el numero de corredores apuntados
 */
int count_players()
{
  int players = 0;
  int fd_shm;
  int *pids=NULL;
  int i;

  if (( fd_shm = shm_open (FICHERO_SISTEMA , O_RDONLY, 0) ) == -1) {
    perror ( " shm_open en count_players" ) ;
    exit ( EXIT_FAILURE ) ;
  }
  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ, MAP_PRIVATE, fd_shm, 0);
  if(pids == MAP_FAILED){
    close(fd_shm);
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  for(i=0;i<MAX_MINEROS;i++){
    if(pids[i] != 0){
      players++;
    }else{
      break;
    }
  } 
  close(fd_shm);
  munmap(pids, MAX_MINEROS * sizeof(int));
  return players;
}

/**
 * @brief La funcion que llama el proceso ganador para hacer esperas
 * cortas no activas hasta que todos hayan votado o haya ocurrido un numero
 * límite de comprobaciones
 *
 * @param mutex_votacion el mutex que protege el fichero en el que los procesos votan
 * @param mutex_pids el mutex que protege el fichero del sistema, donde los procesos se apuntan
 * @param yesNo el array donde la funcion guarda las votaciones
 */
void wait_votation(sem_t *mutex_votacion, int corredores, int yesNo[2])
{
  int i = 0;
  int j = 0;
  int votacionTerminada = 0;
  int votados;
  int fd_shm;
  yesNo[0] = 0;
  yesNo[1] = 0;
  char *votations = NULL;

  // definicion de la estructura para el wait corto
  struct timespec ts = {
      .tv_sec = 0,
      .tv_nsec = 10000000};

  // Si no existe el fichero de votacion , lo creamos limpio
  // sem_wait(mutex_votacion);
  // if (!(votacion = fopen(FICHERO_VOTACION, "r")))
  // {
  //   votacion = fopen(FICHERO_VOTACION, "w");
  // }
  // fclose(votacion);
  // sem_post(mutex_votacion);

  // eliminamos a uno de lo corredores, sera el propio ganador, que no vota
  corredores--;
  while (i < MAX_INTENTOS && !votacionTerminada)
  {
    votados = 0;

    // leemos cuantos han votado
    sem_wait(mutex_votacion);
    if (( fd_shm = shm_open (FICHERO_VOTACION , O_RDWR, 0) ) == -1) {
      perror ( " shm_open wait_votation " ) ;
      exit ( EXIT_FAILURE ) ;
    }
    votations = mmap(NULL, MAX_MINEROS * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if(votations == MAP_FAILED){
      close(fd_shm);
      perror("mmap en wait_votation");
      exit(EXIT_FAILURE);
    }
    for(j=0;j<MAX_MINEROS;j++){
      if(votations[j] != 0){
        votados++;
      }else{
        break;
      }
    } 
    close(fd_shm);
    munmap(votations, MAX_MINEROS * sizeof(char));
    sem_post(mutex_votacion);

    if (votados == corredores)
      votacionTerminada = 1;
    // esperamos 10  milisegundos a probar otra vez
    nanosleep(&ts, NULL);
    i++;
  }

  // una vez terminada la votacion verificamos si es correcta y damos moneda
  if (( fd_shm = shm_open (FICHERO_VOTACION , O_RDWR, 0) ) == -1) {
    perror ( " shm_open fichero votacion " ) ;
    exit ( EXIT_FAILURE ) ;
  }
  votations = mmap(NULL, MAX_MINEROS * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if(votations == MAP_FAILED){
    close(fd_shm);
    perror("mmap en wait_votation2");
    exit(EXIT_FAILURE);
  }
  for(i=0;i<MAX_MINEROS;i++){
    if(votations[i] != 0){
      if(votations[i] == YES) yesNo[0]++;
      else yesNo[1]++;
    }else{
      break;
    }
  } 


  // reseteamos las votaciones para la siguiente ronda truncando el fichero
  memset(votations, 0, MAX_MINEROS);

  close(fd_shm);
  munmap(votations, MAX_MINEROS * sizeof(char));
  sem_post(mutex_votacion);
}

/**
 * @brief Libera todos los recursos creados para la ejecución de hilos
 *
 * @param n_threads indica el número de hilos creados
 * @param arg_array array de todas las estructuras creadas para ser pasadas como
 * argumento a los hilos
 * @param thread_array array de los identificadores de los hilos
 * @params semaphores
 */
void clean_and_free(int n_threads, ArgsSolucion **arg_array,
                    pthread_t *thread_array, sem_t *mutex_pids, sem_t *mutex_target, sem_t *ganador_sem, sem_t *mutex_votacion)
{
  int k;
  for (k = 0; k < n_threads && arg_array; k++)
  {
    if ((arg_array)[k])
      free(arg_array[k]);
  }

  if (thread_array != NULL)
    free(thread_array);
  if (arg_array != NULL)
    free(arg_array);

  // cerramos los mutex
  sem_close(mutex_pids);
  sem_close(mutex_target);
  sem_close(ganador_sem);
  sem_close(mutex_votacion);
}


/**
 * @brief aplica la función hash a todos los valores entre un intervalo dado
 * @param arg estructura en la que el hilo recibe toda la información que
 * necesita para ejecutarse correctamente
 * @return int el valor que satisface la solución hash
 */
void *buscar_solucion(void *arg)
{
  long i = 0;
  long n_valores, n_hilo, target, *result;

  /*Casteamos el array a su tipo correcto*/
  ArgsSolucion argssolucion;
  argssolucion = *((ArgsSolucion *)arg);

  /*Leemos los datos de los argumentos*/
  n_valores = argssolucion.n_valores;
  n_hilo = argssolucion.n_hilo;
  target = argssolucion.target;

  /*Reservamos memoria para la solución que devolveremos*/
  result = (long *)malloc(sizeof(long));

  /*Iteramos todos los valores posibles para encontrar el deseado*/
  for (i = n_valores * n_hilo;
       i < n_valores * (n_hilo + 1) && !finished; i++)
  {
    if (pow_hash(i) == target)
    {
      finished = 1;
      *result = i;
      pthread_exit(result);
    }
  }
  *result = SOLUTION_NOT_FOUND;
  pthread_exit(result);
}

int main(int argc, char *argv[])
{
  long pid = 0;
  int i = 0, j = 0, k = 0;
  long interval = 0;
  int n_secs = 0;
  int n_threads = 0;
  int error;
  long solution;
  void *retval = NULL;
  int ret_int;
  int yesNo[2];
  sem_t *mutex_pids = NULL;
  sem_t *mutex_target = NULL;
  sem_t *ganador_sem = NULL;
  sem_t *mutex_votacion = NULL;
  sigset_t origMask, block2mask, block1mask;
  int n_miners;
  int ganador = 0;
  int target;
  int rondas_ganadas = 0, rondas_verificadas = 0, rondas_corridas = 0;

  /**Estructura para el nanosleep*/
  struct timespec ts = {
      .tv_sec = 0,
      .tv_nsec = 10000000};

  struct sigaction act_alarm, act_sigusr1, act_sigusr2;

  pthread_t h, *thread_array = NULL;
  ArgsSolucion **arg_array = NULL;

  // Semilla para el numero aleatorio utilizado en las votaciones
  srand(time(NULL));

  // Descriptores para el pipe del registrador
  int minero_escribe[2];
  int registrador_escribe[2];
  char buffer[1024];
  int fd;
  char validado[] = "validated";
  char rejected[] = "rejected";
  char *status = NULL;

  /*Tratamiento de los argumentos de entrada*/
  if (argc != 3)
  {
    printf("Not enough arguments for the program\n");
    return EXIT_FAILURE;
  }
  n_secs = atoi(argv[1]);
  n_threads = atoi(argv[2]);

  if (n_threads <= 0 || n_secs < 0)
  {
    printf("Argumentos erróneos");
    exit(EXIT_FAILURE);
  }
  /*Apertura del pipe*/
  pipe(minero_escribe);
  pipe(registrador_escribe);

  /*Aplicamos la máscara que será empleada por los mineros*/
  /*Bloqueamos la señal 1, pues será la que podrán recibir los mineros que no lideren las carreras
    De esta manera nos aseguramos que la señal no se puede perder antes de que un proceso llegue a sigsuspend con la otra mascara*/
  sigemptyset(&block1mask);
  sigaddset(&block1mask, SIGUSR1);
  if (pthread_sigmask(SIG_BLOCK, &block1mask, &origMask) == -1)
  {
    perror("pthread_procmask");
  }
  /*Preparamos la máscara que desbloquea SIGUSR1 y bloquea tanto SIGUSR2 como SIGALRM para comenzar la carrera*/
  sigemptyset(&block2mask);
  sigaddset(&block2mask, SIGUSR2);
  sigaddset(&block2mask, SIGALRM);

  /*División de los procesos*/
  pid = fork();
  if (pid < 0)
  {

    close(minero_escribe[0]);
    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    close(registrador_escribe[1]);

    perror("Error en el fork\n");
    return EXIT_FAILURE;
  }
  /*Proceso hijo: registrador*/
  if (pid == 0)
  {
    int round;
    char *pointer;

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
    close(minero_escribe[0]);
    close(registrador_escribe[1]);
    close(fd);
    printf("Register of %d exited with status 0\n", getppid());
    exit(EXIT_SUCCESS);
  }
  /*Proceso padre: minero*/
  else
  {

    /*************************************************
     **************CONFIGURACIONES PREVIAS************
     **************************************************/

    // Configuramos la señal de alarma
    act_alarm.sa_handler = handler_alarm;
    sigemptyset(&(act_alarm.sa_mask));
    //SA_RESTART para indicar que si un wait es interrumpido por señales, debe volver a la espera y no saltarselo
    act_alarm.sa_flags = SA_RESTART;

    if (sigaction(SIGALRM, &act_alarm, NULL) < 0)
    {
      perror(" sigaction ");
      exit(EXIT_FAILURE);
    }

    // Configuramos el manejador para sigursr1
    act_sigusr1.sa_handler = handler_sigusr1;
    sigemptyset(&(act_sigusr1.sa_mask));
    act_sigusr1.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR1, &act_sigusr1, NULL) < 0)
    {
      perror(" sigaction ");
      exit(EXIT_FAILURE);
    }
    // Configuramos el manejador para sigursr2
    act_sigusr2.sa_handler = handler_sigusr2;
    sigemptyset(&(act_sigusr2.sa_mask));
    act_sigusr2.sa_flags = SA_RESTART;

    if (sigaction(SIGUSR2, &act_sigusr2, NULL) < 0)
    {
      perror(" sigaction ");
      exit(EXIT_FAILURE);
    }

    /**Se cierran pipes pertinentes*/
    close(registrador_escribe[1]); /*registrador escribe (escritura)*/
    close(minero_escribe[0]);      /*minero escribe (lectura)*/

    /**Abrimos todos los semaforos para la ejecucion de las tareas coordinadas */

    if ((mutex_votacion = sem_open(MUTEX_VOTACION_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
        SEM_FAILED)
    {
      perror("sem_open");
      exit(EXIT_FAILURE);
    }

    if ((mutex_pids = sem_open(MUTEX_PIDS_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
        SEM_FAILED)
    {
      perror("sem_open");
      clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion);
      exit(EXIT_FAILURE);
    }

    if ((ganador_sem = sem_open(GANADOR_SEM, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
        SEM_FAILED)
    {
      perror("sem_open");
      clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion);
      exit(EXIT_FAILURE);
    }

 
    if ((mutex_target = sem_open(MUTEX_TARGET_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
        SEM_FAILED)
    {
      perror("sem_open");
      clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion);
      exit(EXIT_FAILURE);
    }

    // Iniciamos la cuenta con la alarma
    alarm(n_secs);

    /*Accedemos al fichero de los pids*/
    sem_wait(mutex_pids); /*Accedemos a sección crítica: el fichero de pid's*/
    inscribirseLista(getpid());
    sem_post(mutex_pids);

    /*Calculamos el intervalo de valores que recorrerá cada hilo*/
    interval = (POW_LIMIT - 1) / n_threads + 1;

    /*Reservamos la memoria necesaria para el programa*/
    arg_array = (ArgsSolucion **)malloc(sizeof(ArgsSolucion *) * n_threads);
    thread_array = (pthread_t *)malloc(sizeof(pthread_t) * n_threads);

    if (!arg_array || !thread_array)
    {
      close(minero_escribe[1]);
      close(registrador_escribe[0]);
      sem_wait(mutex_pids); /*Accedemos a sección crítica: el fichero de pid's*/
      desinscribirseLista(getpid());
      sem_post(mutex_pids);
      clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion);
      printf("miner <%d> exited with status 1\n", getpid());
      wait(NULL);
      exit(EXIT_FAILURE);
    }

    for (i = 0; i < n_threads; i++)
    {
      arg_array[i] = (ArgsSolucion *)malloc(sizeof(ArgsSolucion));
      if (!arg_array[i])
      {
        close(minero_escribe[1]);
        close(registrador_escribe[0]);
        sem_wait(mutex_pids); /*Accedemos a sección crítica: el fichero de pid's*/
        desinscribirseLista(getpid());
        sem_post(mutex_pids);
        clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion);
        printf("miner <%d> exited with status 1\n", getpid());
        wait(NULL);
        exit(EXIT_FAILURE);
      }
      arg_array[i]->n_valores = interval;
      arg_array[i]->n_hilo = i;
    }

    /*Accedemos al fichero de los targets*/

    sem_wait(mutex_target);
    target = leer_target();
    /*Si no hay un target, lo añadimos nosotros y somos nosotros los que iniciamos la primera carrera*/
    if (target == NO_TARGET)
    {
      new_target(0);
      target = 0;
      ganador = 1;
    }
    sem_post(mutex_target);

    /*************************************/
    /******BUCLE DE EJECUCION*************/
    /*************************************/

    /*Ejecutamos el código del minero*/
    while (terminar == 0)
    {
      // Corremos una ronda más
      rondas_corridas++;
      // restauramos el valor de finished para la siguiente ronda
      finished = 0;

      if (ganador)
      {
        // El proceso ganador (o el primero que llegue), espera a que haya mas jugadores esperando para correr

        sem_wait(mutex_pids);
        n_miners = count_players();
        sem_post(mutex_pids);
        while (n_miners < 2 && !terminar)
        {
          nanosleep(&ts, NULL);
          sem_wait(mutex_pids);
          n_miners = count_players();
          sem_post(mutex_pids);
        }
        // el primer proceso ha muerto en la espera de otros corredores
        if (terminar)
          continue;

        // empezamos al carrera

        sem_wait(mutex_pids);
        start_race(getpid());
        sem_post(mutex_pids);
      }
      else
      {
        /*Esperamos a recibir la señal de comienzo de la carrera*/
        /*Aplicamos la máscara que bloqueará la señal dos, que es la que los mineros que pierdan aplicarán posteriormente*/
        /*Le pasamos así una máscara en la que no aparece SIGUSR1 para que el prceso se desbloquee con dicha señal*/

        // Este sigsuspend tambien detecta alarma, por lo que si a un proceso le suena, empezará a ejecutarse
        sigsuspend(&block2mask);

      }

      /******************************/
      /**********CARRERA*************/
      /******************************/

      // Nos guardamos el numero de mineros que compiten en esta ronda, para esperar en la votacion


      sem_wait(mutex_pids);
      n_miners = count_players();

      sem_post(mutex_pids);
      for (j = 0; j < n_threads; j++)
      {
        /*Preparar el argumento del hilo*/
        arg_array[j]->target = target;

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
          close(registrador_escribe[0]);
          wait(NULL);
          printf("Miner <%d> exited with status 1\n", getpid());
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
          /*Nos guardamos la solución que sea correcta, pues los hilos que
           * terminan sin encontrar una solución devuelven -1*/
          solution = *(long *)retval;
        }
        free(retval);
      }

      /*El proceso intenta ser el ganador, si no lo es continua*/
      sem_wait(mutex_target);
      ret_int = sem_trywait(ganador_sem);
      if (ret_int == 0)
      {

        // PROCESO GANADOR, MANDA SEÑAL A LOS PERDEDORES Y ESPERA A QUE VOTEN
        // mandamos señal a todos los demas procesos de terminar la carrera
        sem_wait(mutex_pids);
        start_votation(getpid());
        sem_post(mutex_pids);

        new_target(solution);
        sem_post(mutex_target);
        /*Esperamos a que todos los procesos voten*/
        wait_votation(mutex_votacion, n_miners, yesNo);
        // Sumamos a las rondas ganadas del minero y si gana la votacion sumamos a las rondas verificadas
        rondas_ganadas++;
        if (yesNo[0] >= yesNo[1])
        {
          rondas_verificadas++;
          status = validado;
        }
        else
        {
          status = rejected;
        }
        ganador = 1;
        // Imprimimos por terminal
        printf("Winner <%d> Yes|No: (%d|%d) => %s\n", getpid(), yesNo[0], yesNo[1], yesNo[0] >= yesNo[1] ? "Accepted" : "Rejected");
        // Mandamos al registrador también la información
        sprintf(buffer, "%d|%d|%ld|%d|%d|%d|%s\n", rondas_corridas, target, solution, yesNo[0], yesNo[1], rondas_verificadas, status);
        write(minero_escribe[1], buffer, strlen(buffer) + 1);

        // Escribimos el nuevo valor para el target
        target = solution;

        /**Leemos la señal del registrador para continuar con la siguiente ronda
         */
        if (read(registrador_escribe[0], buffer, sizeof(buffer)) <= 0)
        {
          close(minero_escribe[1]);
          close(registrador_escribe[0]);
          sem_wait(mutex_pids);
          desinscribirseLista(getpid());
          n_miners = count_players();
          sem_post(mutex_pids);

          if (n_miners == 0)
          {
            printf("ELIMINANDO FICHEROS <%d>\n", getpid());
            unlink(FICHERO_SISTEMA);
            unlink(FICHERO_TARGET);
            unlink(FICHERO_VOTACION);
            sem_unlink(MUTEX_PIDS_SEM_NAME);
            sem_unlink(MUTEX_TARGET_SEM_NAME);
            sem_unlink(GANADOR_SEM);
            sem_unlink(MUTEX_VOTACION_SEM_NAME);
          }
          clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion);
          printf("Miner exited with status 0\n");
          wait(NULL);
          return EXIT_SUCCESS;
        }

        sem_post(ganador_sem);
      }
      else
      {
        // PROCESO PERDEDOR, ESPERA A CAMBIO DE TARGET Y VOTA
        // leemos el nuevo target y hacemos votacion
        target = leer_target();
        sem_post(mutex_target);
        sem_wait(mutex_votacion);
        votar(target);
        sem_post(mutex_votacion);
        // Consta que no es el ganador para empezar la siguiente ronda
        ganador = 0;
      }
    }

    /**********FINAL DE EJECUCION Y MUERTE DEL PROCESO**************/

    // Si el proceso que sale es el ganador, procede a mandarle la señal de comienzo de la carrera a los demas corredores
    // esto para evitar bloqueos
    if (ganador == 1)
    {
      sem_wait(mutex_pids);
      start_race(getpid());
      desinscribirseLista(getpid());
      n_miners = count_players();
      sem_post(mutex_pids);
    }
    else
    {
      sem_wait(mutex_pids);
      desinscribirseLista(getpid());
      n_miners = count_players();
      sem_post(mutex_pids);
    }
    // Vemos que la seccion critica unifica desinscribirse y countplayers. Esto soluciona el caso de que dos se desapunten y lean 0 personas a la vez
    // Cuando el primero en desapuntarse deberia haber leido que quedaba uno


    if (n_miners == 0)
    {
      printf("ELIMINANDO FICHEROS <%d>\n", getpid());
      unlink(FICHERO_SISTEMA);
      unlink(FICHERO_TARGET);
      unlink(FICHERO_VOTACION);

      sem_unlink(MUTEX_PIDS_SEM_NAME);
      sem_unlink(MUTEX_TARGET_SEM_NAME);
      sem_unlink(GANADOR_SEM);
      sem_unlink(MUTEX_VOTACION_SEM_NAME);
    }

    // limpiamos ejecucion
    clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion);

    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    // clean_and_free(n_threads, arg_array, thread_array);
    wait(NULL);
    printf("Miner <%d> exited with status 0\n", getpid());
    return EXIT_SUCCESS;
  }
}
