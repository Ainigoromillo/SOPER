/**
 * @file monitor.c
 * @author Alvaro Iñigo y Matteo Artuñedo
 * @brief implementa al monitor, encargado de crear las estructuras necesarias para que
 * los mineros puedan realizar sus tareas y recoge sus resultados
 * @version 0.1
 * @date 2026-05-03
 *
 */
#include "resources.h"

/**
 * Estructura que almacena todos los semáforos que usará el sistema, los cuales son creados y destruidos por el monitor
 */
typedef struct{
  sem_t *mutex_votacion;
  sem_t *mutex_pids;
  sem_t *ganador_sem;
  sem_t *mutex_target;
}SystemSemaphores;

typedef struct{
  int fd_Pids;
  int fd_Target;
  int fd_Votacion;
}FileDescriptors;

typedef struct {
  int fd_MinerComprobadorQueue;
  int fd_ComprobadorMonitorQueue;
}MessageQueues;

atomic_int interrupted = 0; // Variable global compartida por los hilos que indica que deben dejar de minar

/**Función responsable de gestionar la llegada de señales SIGALRM*/
void handler_alarm(int sig)
{
  interrupted = 1;
}

/*****************************************************/
/********************SEMÁFOROS************************/
/*****************************************************/

void close_semaphores(const SystemSemaphores *sems) {
  sem_close(sems->ganador_sem);
  sem_close(sems->mutex_pids);
  sem_close(sems->mutex_target);
  sem_close(sems->mutex_votacion);
}

/**
 * Método encargado de abrir todos los semáforos del sistema
 */
SystemSemaphores *create_semaphores() {
  SystemSemaphores *sems = NULL;
  if (!((sems = (SystemSemaphores *) malloc(sizeof(SystemSemaphores))))) {
    return NULL;
  }
  if ((sems->mutex_votacion = sem_open(MUTEX_VOTACION_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  if ((sems->mutex_pids = sem_open(MUTEX_PIDS_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  if ((sems->ganador_sem = sem_open(GANADOR_SEM, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }


  if ((sems->mutex_target = sem_open(MUTEX_TARGET_SEM_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }
  /*Una vez creados los semáforos, los cerramos*/
  return sems;
}

void unlink_semaphores() {
  sem_unlink(MUTEX_VOTACION_SEM_NAME);
  sem_unlink(GANADOR_SEM);
  sem_unlink(MUTEX_PIDS_SEM_NAME);
  sem_unlink(MUTEX_TARGET_SEM_NAME);
}

void semaphores_exit(SystemSemaphores *sems) {
  close_semaphores(sems);
  free(sems);
  unlink_semaphores();
}

/*****************************************************/
/**********MEMORIA COMPARTIDA*************************/
/*****************************************************/

void create_fds(FileDescriptors *fds) {
  int *target=NULL;

  fds->fd_Votacion = shm_open ( FICHERO_VOTACION , O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR ) ;
  if ( fds->fd_Votacion != -1 ) {
    if((ftruncate(fds->fd_Votacion, sizeof(char) * MAX_MINEROS)) == -1) {
      perror("ftruncate");
      sem_unlink(FICHERO_VOTACION);
      exit(EXIT_FAILURE);
    }
  }else {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
  close(fds->fd_Votacion);


  fds->fd_Pids = shm_open ( FICHERO_PIDS , O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR ) ;
  if (fds->fd_Pids != -1 ) {
    if((ftruncate(fds->fd_Pids, sizeof(int) * MAX_MINEROS)) == -1){
      perror("ftruncate");
      sem_unlink(FICHERO_PIDS);
      exit(EXIT_FAILURE);
    }
  }else {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
  close(fds->fd_Pids);

  fds->fd_Target = shm_open ( FICHERO_TARGET , O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR ) ;
  if ( fds->fd_Target != -1) {
    if((ftruncate(fds->fd_Target, sizeof(int))) == -1){
      perror("ftruncate");
      sem_unlink(FICHERO_TARGET);
      exit(EXIT_FAILURE);
    }
  }else{
    perror ( " shm_open " ) ;
    exit ( EXIT_FAILURE ) ;
  }

  /*Escribimos el valor inicial del target*/
  target = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fds->fd_Target, 0);
  if(target == MAP_FAILED){
    close(fds->fd_Target);
    perror("mmap en leer_target");
    exit(EXIT_FAILURE);
  }
  *target=NO_TARGET;
  munmap(target, sizeof(int));

  close(fds->fd_Target);
}

void unlink_shared_memory() {
  shm_unlink(FICHERO_PIDS);
  shm_unlink(FICHERO_TARGET);
  shm_unlink(FICHERO_VOTACION);
}

/*****************************************************/
/**********COLAS DE MENSAJES**************************/
/*****************************************************/
void close_message_queues(MessageQueues *mqs) {
  mq_close(mqs->fd_ComprobadorMonitorQueue);
  mq_close(mqs->fd_MinerComprobadorQueue);
}

void unlink_message_queues() {
  mq_unlink(COMPROBADOR_MONITOR_MESSAGE_QUEUE);
  mq_unlink(MINER_COMPROBADOR_MESSAGE_QUEUE);
}

void create_message_queues(MessageQueues *mqs) {
  struct mq_attr attributes;
  attributes . mq_maxmsg = 10;
  attributes . mq_msgsize = MAX_MESSAGE ;
  if ((mqs->fd_ComprobadorMonitorQueue = mq_open(COMPROBADOR_MONITOR_MESSAGE_QUEUE, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attributes)) ==
      (mqd_t)-1)
  {
    perror("mq_open");
    exit(EXIT_FAILURE);
  }
  if ((mqs->fd_MinerComprobadorQueue = mq_open(MINER_COMPROBADOR_MESSAGE_QUEUE, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attributes)) ==
       (mqd_t)-1)
  {
    perror("mq_open");
    exit(EXIT_FAILURE);
  }
}

/*****************************************************/
/***********************MAIN**************************/
/*****************************************************/

int main(int argc, char *argv[])
{
  int pid,lag_comprobador, lag_monitor;
  struct sigaction act_alarm;
  SystemSemaphores *sems=NULL;
  FileDescriptors fds;
  MessageQueues mqs;
  sigset_t mask, oldmask;
  char aux[MAX_MESSAGE];
  char *word1=NULL;
  char *word2=NULL;

  /*Tratamiento de los argumentos de entrada*/
  if (argc != 3)
  {
    printf("Not enough arguments for the program\n");
    return EXIT_FAILURE;
  }
  lag_comprobador = atoi(argv[1]);
  lag_monitor = atoi(argv[2]);

  /*Creamos los semáforos del sistema*/
  if ((sems = create_semaphores()) == NULL) {
    fprintf(stderr, "Los semáforos no se han podido crear correctamente");
    exit(EXIT_FAILURE);
  }

  /*Creamos los ficheros de memoria compartida del sistema*/
  create_fds(&fds);

  /*Creamos los ficheros de memoria compartida del sistema*/
  create_message_queues(&mqs);

  /*Realizamos el bloqueo de la señal de alarma, pues ambos procesos usarán la alarma para su temporización y la máscara
     *de señales bloqueadas y el handler se heredan tras un fork*/
  sigemptyset (&mask);
  sigaddset (&mask, SIGUSR1);
  /*Bloqueamos la señal SIGUSR1 antes de hacer el sigaction para que no se pierda*/
  sigprocmask (SIG_BLOCK, &mask, &oldmask);
  // Configuramos el handler de la señal de alarma

  /*División de los procesos*/
  pid = fork();
  if (pid < 0)
  {
    perror("Error en el fork\n");
    semaphores_exit(sems);
    return EXIT_FAILURE;
  }
  /*Proceso hijo: monitor*/
  if (pid == 0)
  {
    /*El monitor no necesita la cola que existe entre los mineros y el comprobador*/
    mq_close(mqs.fd_MinerComprobadorQueue);
    close_semaphores(sems);
    free(sems);
    close_message_queues(&mqs);
    exit(EXIT_SUCCESS);
  }
  /*Proceso padre: comprobador*/
  else {
    int finishCondition = 0;
    //Usamos setitimer para poder establecer intervalos de milisegundos, pues con alarm solo se pueden usar segundos
    struct itimerspec value;
    timer_t timerid;
    struct sigevent evp = {0};

    //Introducimos el tiempo en nanosegundos
    value.it_value.tv_nsec = lag_comprobador * 1000000;
    value.it_value.tv_sec = lag_comprobador * 1000000;

    //Configuramos la señal que se lanzará una vez transcurrido el timer
    evp.sigev_notify = SIGEV_SIGNAL;
    evp.sigev_signo = SIGALRM;


    //Creamos el timer
    timer_create(CLOCK_REALTIME, &evp, &timerid);


    act_alarm.sa_handler = handler_alarm;
    sigemptyset(&(act_alarm.sa_mask));
    if (sigaction(SIGALRM, &act_alarm, NULL) < 0)
    {
      perror(" sigaction ");
      exit(EXIT_FAILURE);
    }

    /* Set up the mask of signals to temporarily block. */

    while (finishCondition == 0) {
      //Hacemos que empiece a contar el tiempo
      timer_settime(timerid, TIMER_ABSTIME, &value, NULL);
      /*Esperamos un mensaje de algún minero*/
      if ((mq_receive(mqs.fd_MinerComprobadorQueue, aux, MAX_MESSAGE, NULL)) == (mqd_t)-1) {
        perror(("mq_receive"));
        exit(EXIT_FAILURE);
      }
      /*Analizamos el mensaje*/
       word1 = strtok(aux, " ");
       if (strcmp(word1, MINERS_ENDED) == 0) {
         finishCondition = 1;
         printf("Finish condition recibida\n");
       }else {
         word2 = strtok(NULL, " ");
         printf("%s %s\n", word1, word2);
         while (!interrupted) {
           sigsuspend(&oldmask);
         }
       }
    }
  }


  wait(NULL);
  semaphores_exit(sems);
  unlink_shared_memory();
  close_message_queues(&mqs);
  unlink_message_queues();
  return EXIT_SUCCESS;
}
