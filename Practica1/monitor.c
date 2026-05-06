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


#define FICHERO_SISTEMA "/systemFile"

#define MSG_QUEUE_TAM 7
#define VALIDATED 1
#define END_OF_PROGRAM -1

/**
 * Estructura que almacena todos los semáforos que usará el sistema, los cuales son creados y destruidos por el monitor
 */
typedef struct{

  //Semaforos de minero-registrador
  sem_t *mutex_votacion;
  sem_t *mutex_pids;
  sem_t *ganador_sem;
  sem_t *mutex_target;


}SystemSemaphores;




typedef struct{
  int fd_Pids;
  int fd_Target;
  int fd_Votacion;
  int fd_monitorYComprobador;
}FileDescriptors;

typedef struct {
  int target;
  long solution;
  int validated;
}InfoParaMonitor;

typedef struct {
  mqd_t fd_MinerComprobadorQueue;
}MessageQueues;


typedef struct{
    //semaforos para monitor-comprobador, notar que no son punteros,
    //puesto que esos punteros serian respectivos a la memoria virtual de uno
    //de los procesos.
  sem_t sem_mutex;
  sem_t sem_fill;
  sem_t sem_empty;

  //buffer de 6 bloques
  InfoParaMonitor cola[6];
  int next, end;

}MemoriaCompartida;

int interrupted = 0; 


/*****************************************************/
/********************SEMÁFOROS************************/
/*****************************************************/

void close_semaphores(const SystemSemaphores *sems) {
  sem_close(sems->ganador_sem);
  sem_close(sems->mutex_pids);
  sem_close(sems->mutex_target);
  sem_close(sems->mutex_votacion);
}

void create_individual_semaphore(sem_t **semaphore, int init_value, char *sem_name) {
  if (((*semaphore) = sem_open(sem_name, O_CREAT, S_IRUSR | S_IWUSR, init_value)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }
}

/**
 * Método encargado de abrir todos los semáforos del sistema
 */
SystemSemaphores *create_semaphores() {
  SystemSemaphores *sems = NULL;
  if (!((sems = (SystemSemaphores *) malloc(sizeof(SystemSemaphores))))) {
    return NULL;
  }


  //Semáforos para los mineros
  create_individual_semaphore(&(sems->ganador_sem), 1, GANADOR_SEM);
  create_individual_semaphore(&(sems->mutex_pids), 1, MUTEX_PIDS_SEM_NAME);
  create_individual_semaphore(&(sems->mutex_target), 1, MUTEX_TARGET_SEM_NAME);
  create_individual_semaphore(&(sems->mutex_votacion), 1, MUTEX_VOTACION_SEM_NAME);

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

int abrir_descriptor_compartida(){
  int fd = shm_open ( FICHERO_SISTEMA , O_RDWR | O_CREAT | O_EXCL , S_IRUSR | S_IWUSR ) ;
  MemoriaCompartida *shm = NULL;

  if (fd != -1 ) {
    if((ftruncate(fd, sizeof(MemoriaCompartida))) == -1){
      perror("ftruncate");
      sem_unlink(FICHERO_SISTEMA);
      exit(EXIT_FAILURE);
    }

    shm = mmap(NULL, sizeof(MemoriaCompartida), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) {
    perror("mmap");
    shm_unlink(FICHERO_SISTEMA);
    exit(EXIT_FAILURE);
}
    //ahora abrimos los semaforos sin nombre
    sem_init(&shm->sem_mutex, 1, 1);  // mutex , empieza en 1
    sem_init(&shm->sem_fill,  1, 0);  // fill , empieza en 0 (buffer vacío)
    sem_init(&shm->sem_empty, 1, 6);  // empty , empieza en 6 (6 huecos libres)
    shm->end = 0;
    shm->next = 0;

     munmap(shm, sizeof(MemoriaCompartida)); //desmapeo y devuelvo
     return fd;

  }else {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }

}

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
      sem_unlink(FICHERO_VOTACION);
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
      sem_unlink(FICHERO_VOTACION);
      sem_unlink(FICHERO_PIDS);
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


  //abrimos el descriptor de memoria compartida entre monitor-comprobador y lo dejamos abierto para acceder mas adelante
  fds->fd_monitorYComprobador = abrir_descriptor_compartida();
  

}



void unlink_shared_memory() {
  shm_unlink(FICHERO_PIDS);
  shm_unlink(FICHERO_TARGET);
  shm_unlink(FICHERO_VOTACION);
  shm_unlink(FICHERO_SISTEMA);
}

void close_memoriaCompartida(FileDescriptors *fds){
  MemoriaCompartida *shm = NULL;
  
  shm = mmap(NULL, sizeof(MemoriaCompartida), PROT_WRITE, MAP_SHARED, fds->fd_monitorYComprobador, 0);
    if (shm == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
}

  sem_destroy(&shm->sem_empty);
  sem_destroy(&shm->sem_fill);
  sem_destroy(&shm->sem_mutex);
  munmap(shm, sizeof(MemoriaCompartida));

  close(fds->fd_monitorYComprobador);

  return;
 }

/*****************************************************/
/**********COLAS DE MENSAJES**************************/
/*****************************************************/
void close_message_queues(MessageQueues *mqs) {;
  mq_close(mqs->fd_MinerComprobadorQueue);
}

void unlink_message_queues() {
  mq_unlink(COMPROBADOR_MONITOR_MESSAGE_QUEUE);
  mq_unlink(MINER_COMPROBADOR_MESSAGE_QUEUE);
}

void create_message_queues(MessageQueues *mqs) {
  struct mq_attr attributes;
  attributes . mq_maxmsg = MSG_QUEUE_TAM;
  attributes . mq_msgsize = MAX_MESSAGE ;

  if ((mqs->fd_MinerComprobadorQueue = mq_open(MINER_COMPROBADOR_MESSAGE_QUEUE, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attributes)) ==
       (mqd_t)-1)
  {
    perror("mq_open");
    exit(EXIT_FAILURE);
  }
}

/****************************************************/
/*************Funciones del comprobador**************/
/****************************************************/
void escribir_mensaje(SystemSemaphores *sems, FileDescriptors *fds, InfoParaMonitor info) {
  MemoriaCompartida *shm = NULL;
  

  shm = mmap(NULL, sizeof(MemoriaCompartida), PROT_READ | PROT_WRITE, MAP_SHARED, fds->fd_monitorYComprobador, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
}

  sem_wait(&shm->sem_empty);
  sem_wait(&shm->sem_mutex);
  //Escribimos los datos

   
  shm->cola[shm->next] = info;
 
  //muevo el puntero de la cola
  shm->next = (shm->next + 1) % 6;
  
  sem_post(&shm->sem_mutex);
  sem_post(&shm->sem_fill);

  munmap(shm, sizeof(MemoriaCompartida));

}

/****************************************************/
/*************Funciones del monitor******************/
/****************************************************/

void validar_sol(InfoParaMonitor *info){
  info->validated = 1;
 }


void leer_mensaje(SystemSemaphores *sems, FileDescriptors *fds, InfoParaMonitor *info) {
  MemoriaCompartida *shm = NULL;
  
 

  shm = mmap(NULL, sizeof(MemoriaCompartida), PROT_READ | PROT_WRITE, MAP_SHARED, fds->fd_monitorYComprobador, 0);
  if (shm == MAP_FAILED) {
    perror("mmap");
    exit(EXIT_FAILURE);
}

  //Leemos los datos
  sem_wait(&shm->sem_fill);
  sem_wait(&shm->sem_mutex);



  *info = shm->cola[shm->end];
    
  shm->end = (shm->end + 1) % 6;

  sem_post(&shm->sem_mutex);
  sem_post(&shm->sem_empty);

  munmap(shm, sizeof(MemoriaCompartida));

}

/*****************************************************/
/***********************MAIN**************************/
/*****************************************************/

int main(int argc, char *argv[])
{
  int pid;
  SystemSemaphores *sems=NULL;
  FileDescriptors fds;
  MessageQueues mqs;
  sigset_t mask, oldmask;
  char aux[MAX_MESSAGE];
  char *word1=NULL;
  char *word2=NULL;
  InfoParaMonitor informacion;
  int finishCondition = 0;
  struct timespec lag_comprobador, lag_monitor;
 lag_comprobador.tv_sec = 0;
 lag_monitor.tv_sec = 0;


  /*Tratamiento de los argumentos de entrada*/
  if (argc != 3)
  {
    printf("Not enough arguments for the program\n");
    return EXIT_FAILURE;
  }
  lag_comprobador.tv_nsec = atoi(argv[1])* 1000000;
  lag_monitor.tv_nsec = atoi(argv[2])* 1000000;



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
    close_message_queues(&mqs);

    while(finishCondition == 0){
      //leemos el mensaje del comprobador
      leer_mensaje(sems, &fds, &informacion);

      //validamos info status e imprimimos
      switch (informacion.validated)
      {

      case END_OF_PROGRAM:
        finishCondition = 1;
        break;

      case VALIDATED:
        printf("Solution accepted: %08d -->%08ld\n", informacion.target, informacion.solution);
        break;
      
      default:
        printf("Solution rejected: %08d !->%08ld\n", informacion.target, informacion.solution);
        break;
      }

      nanosleep(&lag_monitor, NULL);
    }


    close_semaphores(sems);
    close(fds.fd_monitorYComprobador);
    free(sems);

   
  }


  /*Proceso padre: comprobador*/
  else {
    
   
    /* Set up the mask of signals to temporarily block. */

    while (finishCondition == 0) {
     
      /*Esperamos un mensaje de algún minero*/
      if ((mq_receive(mqs.fd_MinerComprobadorQueue, aux, MAX_MESSAGE, NULL)) == (mqd_t)-1) {
        perror(("mq_receive"));
        exit(EXIT_FAILURE);
      }
      /*Analizamos el mensaje*/
       word1 = strtok(aux, " ");
       if (strcmp(word1, MINERS_ENDED) == 0) {
         finishCondition = 1;
         //mandamos mensaje de final de programa al monitor
         informacion.validated = END_OF_PROGRAM;
         escribir_mensaje(sems, &fds, informacion );

       }else {
         word2 = strtok(NULL, " ");

         //mandamos el mensaje por memoria compartida hacia el monitor
        informacion.target = atoi(word1);
        informacion.solution = atol(word2);
        validar_sol(&informacion);
        escribir_mensaje(sems, &fds, informacion );

         //esperamos a la siguiente ronda
         nanosleep(&lag_comprobador, NULL);
         
         
       }
    }

  wait(NULL);
  semaphores_exit(sems);
  close_memoriaCompartida(&fds);

  unlink_shared_memory();
  close_message_queues(&mqs);
  unlink_message_queues();

  }

  exit(EXIT_SUCCESS);
  
}
