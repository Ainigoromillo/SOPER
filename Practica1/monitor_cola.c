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

#include "pow.h"

#define FICHERO_SISTEMA "/systemFile"

#define COMPROBADOR_MONITOR_QUEUE "/comprobadoramonitor"
#define MSG_QUEUE_TAM 7
#define VALIDATED 1
#define END_OF_PROGRAM -1

volatile atomic_int interrupted_monitor = 0;

void handler_sigint(int sig)
{
  interrupted_monitor = 1;
}
/**
 * Estructura que almacena todos los semáforos que usará el sistema, los cuales son creados y destruidos por el monitor
 */
typedef struct
{

  // Semaforos de minero-registrador
  sem_t *mutex_votacion;
  sem_t *mutex_pids;
  sem_t *ganador_sem;
  sem_t *mutex_target;

  sem_t *mutex_wallets;

} SystemSemaphores;

typedef struct
{
  // memoria que usan los mineros
  int fd_Pids;
  int fd_Target;
  int fd_Votacion;
  int fd_wallets;

  // memoria compartida entre comprobador-monitor
  int fd_monitorYComprobador;
} FileDescriptors;

typedef struct
{
  int target;
  long solution;
  int validated;
} InfoParaMonitor;

typedef struct
{
  mqd_t fd_MinerComprobadorQueue;
  mqd_t fd_ComprobadorMonitorQueue;
} MessageQueues;

typedef struct
{
  // semaforos para monitor-comprobador, notar que no son punteros,
  // puesto que esos punteros serian respectivos a la memoria virtual de uno
  // de los procesos.
  sem_t sem_mutex;
  sem_t sem_fill;
  sem_t sem_empty;

  // buffer de 6 bloques
  InfoParaMonitor cola[6];
  int next, end;

} MemoriaCompartida;

/*****************************************************/
/********************SEMÁFOROS************************/
/*****************************************************/

/**
 * @brief Cierra los semaforos, no los elimina
 *
 * @param sems
 */
void close_semaphores(const SystemSemaphores *sems)
{
  sem_close(sems->ganador_sem);
  sem_close(sems->mutex_pids);
  sem_close(sems->mutex_target);
  sem_close(sems->mutex_votacion);
  sem_close(sems->mutex_wallets);
}

/**
 * @brief Create a individual semaphore object
 *
 * @param semaphore
 * @param init_value
 * @param sem_name
 */
void create_individual_semaphore(sem_t **semaphore, int init_value, char *sem_name)
{
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
SystemSemaphores *create_semaphores()
{
  SystemSemaphores *sems = NULL;
  if (!((sems = (SystemSemaphores *)malloc(sizeof(SystemSemaphores)))))
  {
    return NULL;
  }

  // Semáforos para los mineros
  create_individual_semaphore(&(sems->ganador_sem), 1, GANADOR_SEM);
  create_individual_semaphore(&(sems->mutex_pids), 1, MUTEX_PIDS_SEM_NAME);
  create_individual_semaphore(&(sems->mutex_target), 1, MUTEX_TARGET_SEM_NAME);
  create_individual_semaphore(&(sems->mutex_votacion), 1, MUTEX_VOTACION_SEM_NAME);
  create_individual_semaphore(&(sems->mutex_wallets), 1, MUTEX_WALLETS);

  return sems;
}

/**
 * @brief elimina los semaforos que usa el sistema
 *
 */
void unlink_semaphores()
{
  sem_unlink(MUTEX_VOTACION_SEM_NAME);
  sem_unlink(GANADOR_SEM);
  sem_unlink(MUTEX_PIDS_SEM_NAME);
  sem_unlink(MUTEX_TARGET_SEM_NAME);
  sem_unlink(MUTEX_WALLETS);
}

/**
 * @brief La funcion de liberacion de los semaforos antes de terminar el programa
 * por parte del comprobador
 *
 * @param sems
 */
void semaphores_exit(SystemSemaphores *sems)
{
  close_semaphores(sems);
  free(sems);
  unlink_semaphores();
}

/*****************************************************/
/**********MEMORIA COMPARTIDA*************************/
/*****************************************************/

/**
 * @brief Abre el descriptor de la memoria compartida entre comprobador y monitor
 *
 * @return int
 */
int abrir_descriptor_compartida()
{
  int fd = shm_open(FICHERO_SISTEMA, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  MemoriaCompartida *shm = NULL;

  if (fd != -1)
  {
    if ((ftruncate(fd, sizeof(MemoriaCompartida))) == -1)
    {
      perror("ftruncate");
      sem_unlink(FICHERO_SISTEMA);
      exit(EXIT_FAILURE);
    }

    shm = mmap(NULL, sizeof(MemoriaCompartida), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED)
    {
      perror("mmap");
      shm_unlink(FICHERO_SISTEMA);
      exit(EXIT_FAILURE);
    }
    // ahora abrimos los semaforos sin nombre
    sem_init(&shm->sem_mutex, 1, 1); // mutex , empieza en 1
    sem_init(&shm->sem_fill, 1, 0);  // fill , empieza en 0 (buffer vacío)
    sem_init(&shm->sem_empty, 1, 6); // empty , empieza en 6 (6 huecos libres)
    shm->end = 0;
    shm->next = 0;

    munmap(shm, sizeof(MemoriaCompartida)); // desmapeo y devuelvo
    return fd;
  }
  else
  {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Crea los descriptores de memoria compartida para todo el programa,
 * los cierra porque no los usa, pero los crea para los mineros
 *
 * @param fds
 */
void create_fds(FileDescriptors *fds)
{
  int *target = NULL;

  fds->fd_Votacion = shm_open(FICHERO_VOTACION, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (fds->fd_Votacion != -1)
  {
    if ((ftruncate(fds->fd_Votacion, sizeof(char) * MAX_MINEROS)) == -1)
    {
      perror("ftruncate");
      shm_unlink(FICHERO_VOTACION);
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
  close(fds->fd_Votacion);

  fds->fd_Pids = shm_open(FICHERO_PIDS, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (fds->fd_Pids != -1)
  {
    if ((ftruncate(fds->fd_Pids, sizeof(int) * MAX_MINEROS)) == -1)
    {
      perror("ftruncate");
      shm_unlink(FICHERO_VOTACION);
      shm_unlink(FICHERO_PIDS);
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
  close(fds->fd_Pids);

  fds->fd_Target = shm_open(FICHERO_TARGET, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (fds->fd_Target != -1)
  {
    if ((ftruncate(fds->fd_Target, sizeof(int))) == -1)
    {
      perror("ftruncate");
      shm_unlink(FICHERO_VOTACION);
      shm_unlink(FICHERO_PIDS);
      shm_unlink(FICHERO_TARGET);
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    perror(" shm_open ");
    exit(EXIT_FAILURE);
  }

  /*Escribimos el valor inicial del target*/
  target = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fds->fd_Target, 0);
  if (target == MAP_FAILED)
  {
    close(fds->fd_Target);
    perror("mmap en leer_target");
    exit(EXIT_FAILURE);
  }
  *target = NO_TARGET;
  munmap(target, sizeof(int));

  close(fds->fd_Target);

  /*Memoria compartida para los wallets de todos los mineros */
  fds->fd_wallets = shm_open(FICHERO_WALLETS, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if (fds->fd_wallets != -1)
  {
    if ((ftruncate(fds->fd_Pids, sizeof(Wallet) * MAX_MINEROS)) == -1)
    {
      perror("ftruncate");
      shm_unlink(FICHERO_TARGET);
      shm_unlink(FICHERO_VOTACION);
      shm_unlink(FICHERO_PIDS);
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
  close(fds->fd_wallets);

  // abrimos el descriptor de memoria compartida entre monitor-comprobador y lo dejamos abierto para acceder mas adelante
  fds->fd_monitorYComprobador = abrir_descriptor_compartida();
}

/**
 * @brief Elimina los segmentos de memoria compartida que usa todo el programa
 *
 */
void unlink_shared_memory()
{
  shm_unlink(FICHERO_PIDS);
  shm_unlink(FICHERO_TARGET);
  shm_unlink(FICHERO_VOTACION);
  shm_unlink(FICHERO_SISTEMA);
  shm_unlink(FICHERO_WALLETS);
}

/**
 * @brief Cierra la memoria compartida entre el comprobador y el monitor
 *
 * @param fds
 */
void close_memoriaCompartida(FileDescriptors *fds)
{
  MemoriaCompartida *shm = NULL;

  shm = mmap(NULL, sizeof(MemoriaCompartida), PROT_WRITE, MAP_SHARED, fds->fd_monitorYComprobador, 0);
  if (shm == MAP_FAILED)
  {
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

/**
 * @brief Imprimer un ranking de los mineros al terminar la ejecucion del blockchain
 *
 * @param fds
 * @param sems
 */
void ranking_mineros(FileDescriptors *fds, SystemSemaphores *sems)
{
  Wallet *wallets;
  Wallet sorted_wallets[MAX_MINEROS];
  int num_mineros = 0;
  int i, j;
  Wallet tmp;

  fds->fd_wallets = shm_open(FICHERO_WALLETS, O_RDONLY, 0);
  if (fds->fd_wallets == -1)
  {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }

  wallets = mmap(NULL, MAX_MINEROS * sizeof(Wallet), PROT_READ,
                 MAP_SHARED, fds->fd_wallets, 0);
  close(fds->fd_wallets);
  if (wallets == MAP_FAILED)
  {
    perror("mmap en ranking_mineros");
    exit(EXIT_FAILURE);
  }

  sem_wait(sems->mutex_wallets);

  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (wallets[i].pid != 0)
    {
      sorted_wallets[num_mineros].pid = wallets[i].pid;
      sorted_wallets[num_mineros].monedas = wallets[i].monedas;
      num_mineros++;
    }
    else
    {
      break;
    }
  }

  sem_post(sems->mutex_wallets);
  munmap(wallets, MAX_MINEROS * sizeof(Wallet));

  /* Bubble sort descendente por monedas */
  for (i = 0; i < num_mineros - 1; i++)
  {
    for (j = 0; j < num_mineros - 1 - i; j++)
    {
      if (sorted_wallets[j].monedas < sorted_wallets[j + 1].monedas)
      {
        tmp = sorted_wallets[j];
        sorted_wallets[j] = sorted_wallets[j + 1];
        sorted_wallets[j + 1] = tmp;
      }
    }
  }

  printf("=== RANKING MINEROS ===\n");
  for (i = 0; i < num_mineros; i++)
  {
    printf("%d: PID %d -> %d monedas\n",
           i + 1, sorted_wallets[i].pid, sorted_wallets[i].monedas);
  }
}

/*****************************************************/
/**********COLAS DE MENSAJES**************************/
/*****************************************************/
void close_message_queues(MessageQueues *mqs)
{
  ;
  mq_close(mqs->fd_MinerComprobadorQueue);
  mq_close(mqs->fd_ComprobadorMonitorQueue);
}

void unlink_message_queues()
{
  mq_unlink(MINER_COMPROBADOR_MESSAGE_QUEUE);
}

void create_message_queues(MessageQueues *mqs)
{
  struct mq_attr attributes;
  attributes.mq_maxmsg = MSG_QUEUE_TAM;
  attributes.mq_msgsize = MAX_MESSAGE;

  if ((mqs->fd_MinerComprobadorQueue = mq_open(MINER_COMPROBADOR_MESSAGE_QUEUE, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attributes)) ==
      (mqd_t)-1)
  {
    perror("mq_open");
    exit(EXIT_FAILURE);
  }
  if ((mqs->fd_ComprobadorMonitorQueue = mq_open(COMPROBADOR_MONITOR_QUEUE, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, &attributes)) ==
      (mqd_t)-1)
  {
    perror("mq_open");
    exit(EXIT_FAILURE);
  }
}

/****************************************************/
/*************Funciones del comprobador**************/
/****************************************************/
void escribir_mensaje(SystemSemaphores *sems, FileDescriptors *fds, MessageQueues *mqs, InfoParaMonitor info)
{
  char buffer[MAX_MESSAGE];
  sprintf(buffer, "%d|%ld|%d|", info.target, info.solution, info.validated);
  mq_send(mqs->fd_ComprobadorMonitorQueue, buffer, MAX_MESSAGE, 0);
}

/**
 * @brief Funcion que notifica a todos los mineros del sistema que deben terminar su ejecucion para que
 * el comprobador muera y no haya fugas de memoria
 *
 * @param fds
 * @param sems
 */
void interrupt_miners(FileDescriptors *fds, SystemSemaphores *sems)
{

  int *pids_mineros = NULL;
  int i;
  sem_wait(sems->mutex_pids);
  fds->fd_Pids = shm_open(FICHERO_PIDS, O_RDONLY, S_IRUSR | S_IWUSR);
  if (!fds->fd_Pids)
  {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
  pids_mineros = mmap(NULL, sizeof(int) * MAX_MINEROS, PROT_READ, MAP_SHARED, fds->fd_Pids, 0);
  if (pids_mineros == MAP_FAILED)
  {
    perror("mmap");
    exit(EXIT_FAILURE);
  }
  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (pids_mineros[i] != 0)
    {
      printf("mandada alarma a %d\n", pids_mineros[i]);
      kill(pids_mineros[i], SIGALRM);
    }
  }
  munmap(pids_mineros, sizeof(int) * MAX_MINEROS);
  close(fds->fd_Pids);
  sem_post(sems->mutex_pids);
}

/**
 * @brief Funcion con la que el comprobador espera a que todos los mineros hayan muerto
 * y se hayan desapuntado del fichero de pids.
 *
 * @param fds
 * @param sems
 */
void wait_for_miners(FileDescriptors *fds, SystemSemaphores *sems)
{
  int *pids_mineros = NULL;
  struct timespec ts = {
      .tv_sec = 1,
      .tv_nsec = 0};
  fds->fd_Pids = shm_open(FICHERO_PIDS, O_RDONLY, S_IRUSR | S_IWUSR);
  if (!fds->fd_Pids)
  {
    perror("shm_open");
    exit(EXIT_FAILURE);
  }
  pids_mineros = mmap(NULL, sizeof(int) * MAX_MINEROS, PROT_READ, MAP_SHARED, fds->fd_Pids, 0);
  if (pids_mineros == MAP_FAILED)
  {
    perror("mmap");
    exit(EXIT_FAILURE);
  }
  printf("Voy a ver si todavía hay mineros\n");
  sem_wait(sems->mutex_pids);
  while (pids_mineros[0] != 0)
  {
    printf("Todavía hay mineros\n");
    sem_post(sems->mutex_pids);
    nanosleep(&ts, NULL);

    sem_wait(sems->mutex_pids);
  }
  sem_post(sems->mutex_pids);
  munmap(pids_mineros, sizeof(int) * MAX_MINEROS);
  close(fds->fd_Pids);
}

/****************************************************/
/*************Funciones del monitor******************/
/****************************************************/

/**
 * @brief Validar la solucion que ha dado el minero ganador
 *
 * @param info
 */
void validar_sol(InfoParaMonitor *info)
{
  if (info->target == pow_hash(info->solution))
  {
    info->validated = 1;
  }
  else
  {
    info->validated = 0;
  }
}

/**
 * @brief Funcion con la que el monitor lee el mensaje de la memoria compartida con el comprobador
 *
 * @param sems
 * @param fds
 * @param info
 */
void leer_mensaje(SystemSemaphores *sems, FileDescriptors *fds, MessageQueues *mqs, InfoParaMonitor *info)
{
  char buffer[MAX_MESSAGE];
  char *target, *solution, *validated;

  mq_receive(mqs->fd_ComprobadorMonitorQueue, buffer, MAX_MESSAGE, NULL);
  target = strtok(buffer, "|");
  info->target = atoi(target);
  solution = strtok(NULL, "|");
  info->solution = atol(solution);
  validated = strtok(NULL, "|");
  info->validated = atoi(validated);
}

/*****************************************************/
/***********************MAIN**************************/
/*****************************************************/

int main(int argc, char *argv[])
{
  int pid;
  SystemSemaphores *sems = NULL;
  FileDescriptors fds;
  MessageQueues mqs;
  char aux[MAX_MESSAGE];
  char *word1 = NULL;
  char *word2 = NULL;
  InfoParaMonitor informacion;
  int finishCondition = 0;
  struct timespec lag_comprobador, lag_monitor;
  struct sigaction act_sigint;
  lag_comprobador.tv_sec = 0;
  lag_monitor.tv_sec = 0;
  long ms1, ms2;
  /*Tratamiento de los argumentos de entrada*/
  if (argc != 3)
  {
    printf("Not enough arguments for the program\n");
    return EXIT_FAILURE;
  }
  ms1 = atoi(argv[1]);
  ms2 = atoi(argv[2]);

  lag_comprobador.tv_sec = ms1 / 1000;
  lag_comprobador.tv_nsec = (ms1 % 1000) * 1000000L;

  lag_monitor.tv_sec = ms2 / 1000;
  lag_monitor.tv_nsec = (ms2 % 1000) * 1000000L;

  /*Creamos los semáforos del sistema*/
  if ((sems = create_semaphores()) == NULL)
  {
    fprintf(stderr, "Los semáforos no se han podido crear correctamente");
    exit(EXIT_FAILURE);
  }

  /*Creamos los ficheros de memoria compartida del sistema*/
  create_fds(&fds);

  /*Creamos los ficheros de memoria compartida del sistema*/
  create_message_queues(&mqs);

  // Configuramos el handler de la señal de interrupción
  act_sigint.sa_handler = handler_sigint;
  sigemptyset(&(act_sigint.sa_mask));
  act_sigint.sa_flags = 0;
  if (sigaction(SIGINT, &act_sigint, NULL) < 0)
  {

    perror(" sigaction ");
    exit(EXIT_FAILURE);
  }

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

    while (finishCondition == 0 && interrupted_monitor == 0)
    {
      // leemos el mensaje del comprobador
      leer_mensaje(sems, &fds, &mqs, &informacion);
      if (interrupted_monitor == 0)
      {
        // validamos info status e imprimimos
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
      }

      nanosleep(&lag_monitor, NULL);
    }

    close_semaphores(sems);
    close(fds.fd_monitorYComprobador);
    close_message_queues(&mqs);
    free(sems);
  }

  /*Proceso padre: comprobador*/
  else
  {

    while (finishCondition == 0 && interrupted_monitor == 0)
    {

      /*Esperamos un mensaje de algún minero*/
      if (mq_receive(mqs.fd_MinerComprobadorQueue, aux, MAX_MESSAGE, NULL) == (mqd_t)-1)
      {
        if (errno != EINTR)
        {
          perror(("mq_receive"));
          exit(EXIT_FAILURE);
        }
      }

      if (interrupted_monitor)
        break; // si ha llegado la señal de interrupcion mientras esperabamos mensaje, salimos del bucle

      /*Analizamos el mensaje*/
      word1 = strtok(aux, " ");
      if (strcmp(word1, MINERS_ENDED) == 0)
      {
        finishCondition = 1;
        // mandamos mensaje de final de programa al monitor
        informacion.validated = END_OF_PROGRAM;
        escribir_mensaje(sems, &fds, &mqs, informacion);
      }
      else
      {
        word2 = strtok(NULL, " ");

        // mandamos el mensaje por memoria compartida hacia el monitor
        informacion.target = atoi(word1);
        informacion.solution = atol(word2);
        validar_sol(&informacion);
        escribir_mensaje(sems, &fds, &mqs,informacion);

        // esperamos a la siguiente ronda
        nanosleep(&lag_comprobador, NULL);
      }
    }
    if (interrupted_monitor == 1)
    {
      printf("Voy a avisar a los mineros y esperarles\n");
      interrupt_miners(&fds, sems);
      wait_for_miners(&fds, sems);
      printf("Salida por interrupción\n");
    }
    // al final hacemos el ranking de los procesos.
    ranking_mineros(&fds, sems);

    wait(NULL);
    semaphores_exit(sems);
    close_memoriaCompartida(&fds);

    unlink_shared_memory();
    close_message_queues(&mqs);
    unlink_message_queues();
  }

  exit(EXIT_SUCCESS);
}
