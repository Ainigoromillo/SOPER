#include "minero.h"

volatile atomic_int finished = 0; // Variable global compartida por los hilos que indica que deben dejar de minar

#define SOLUTION_NOT_FOUND                                                    \
  -1 /**Valor que devuelven los hilos si no han encontrado una solución para \
        el target */

#define MONITOR_DIED (-2)


/**Variable que indica si el minero ha recibido la señal que indica que ha
 * terminado su tiempo*/
volatile atomic_int terminar = 0;

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
 * @brief Espera un semáforo permitiendo que SIGALRM interrumpa la espera.
 *
 * Si sem_wait se interrumpe por una señal distinta a la alarma, se reintenta.
 * Si se interrumpe porque ya se ha activado terminar, devuelve -1 para que
 * el minero pueda salir sin quedarse bloqueado.
 */
int wait_sem_interrumpible(sem_t *sem)
{
  while (sem_wait(sem) == -1)
  {
    if (errno == EINTR)
    {
      if (terminar)
        return -1;
      continue;
    }

    perror("sem_wait");
    terminar = 1;
    return -1;
  }

  return 0;
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
  int *pids = NULL;
  int i;

  if ((fd_shm = shm_open(FICHERO_PIDS, O_RDONLY, 0)) == -1)
  {
    perror(" shm_open start_race ");
    exit(EXIT_FAILURE);
  }

  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ, MAP_PRIVATE, fd_shm, 0);
  if (pids == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en start_race");
    exit(EXIT_FAILURE);
  }
  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (pids[i] == 0)
      break;
    if (pids[i] != caller_pid)
    {
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
  int *pids = NULL;
  int i;

  if ((fd_shm = shm_open(FICHERO_PIDS, O_RDONLY, 0)) == -1)
  {
    perror(" shm_open start_votation");
    exit(EXIT_FAILURE);
  }

  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ, MAP_PRIVATE, fd_shm, 0);
  if (pids == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en start_votation");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (pids[i] == 0)
      break;
    if (pids[i] != caller_pid)
    {
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
  int *pids = NULL;
  int i;

  fd_shm = shm_open(FICHERO_PIDS, O_RDWR, 0);
  if (fd_shm == -1)
  {
    perror(" Error opening the shared memory segment \n ");
    close(fd_shm);
    exit(EXIT_FAILURE);
  }

  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (pids == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en inscribirseLista");
    exit(EXIT_FAILURE);
  }
  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (pids[i] != 0)
    {
      printf("<%d>\n", pids[i]);
    }
    else
    {
      pids[i] = pid;
      printf("miner <%d> added to the system\n", pid);
      break;
    }
  }
  close(fd_shm);
  munmap(pids, MAX_MINEROS * sizeof(int));
}

/**
 * @brief Funcion con la cual los mineros se apuntan a la lista de wallets (numero de monerdas) de todos los procesos
 * @param pid identificador del minero que se inscribe
 */
void inscribirseWallets(int pid)
{
  int fd_shm;
  Wallet *wallets = NULL;
  int i;

  fd_shm = shm_open(FICHERO_WALLETS, O_RDWR, 0);
  if (fd_shm == -1)
  {
    perror(" Error opening the shared memory segment \n ");
    close(fd_shm);
    exit(EXIT_FAILURE);
  }

  wallets = mmap(NULL, MAX_MINEROS * sizeof(Wallet), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (wallets == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en inscribirseWallets");
    exit(EXIT_FAILURE);
  }
  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (wallets[i].pid != 0)
    {
      continue;
    }
    else
    {
      wallets[i].pid = pid;
      wallets[i].monedas = 0;

      break;
    }
  }
  close(fd_shm);
  munmap(wallets, MAX_MINEROS * sizeof(Wallet));
}

/**
 * @brief Funcion con la cual los mineros se suman una moneda si su ronda es verificada
 * @param pid identificador del minero que se inscribe
 */
void sumarMonedaWallets(int pid)
{
  int fd_shm;
  Wallet *wallets = NULL;
  int i;

  fd_shm = shm_open(FICHERO_WALLETS, O_RDWR, 0);
  if (fd_shm == -1)
  {
    perror(" Error opening the shared memory segment \n ");
    close(fd_shm);
    exit(EXIT_FAILURE);
  }

  wallets = mmap(NULL, MAX_MINEROS * sizeof(Wallet), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (wallets == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en inscribirseWallets");
    exit(EXIT_FAILURE);
  }
  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (wallets[i].pid == pid)
    {
      wallets[i].monedas += 1;
      break;
    }
  }

  close(fd_shm);
  munmap(wallets, MAX_MINEROS * sizeof(Wallet));
}

/**
 * @brief Funcion con la que los procesos votan si la solucion es correcta o no
 *
 * @param solution la solucion obtenida por el proceso ganador
 */
void votar(int target, long solution)
{
  int fd_shm;
  int i;
  char *votations = NULL;

  fd_shm = shm_open(FICHERO_VOTACION, O_RDWR, 0);
  if (fd_shm == -1)
  {
    perror(" Error creating the shared memory segment \n ");
    close(fd_shm);
    exit(EXIT_FAILURE);
  }

  votations = mmap(NULL, MAX_MINEROS * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (votations == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en votar");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (votations[i] == 0)
    {
      votations[i] = pow_hash(solution) == target ? YES : NO;
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
  int *pids = NULL;
  int i;

  if ((fd_shm = shm_open(FICHERO_PIDS, O_RDWR, 0)) == -1)
  {
    perror(" shm_open en desinscribirseLista");
    exit(EXIT_FAILURE);
  }

  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (pids == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en desinscribirseLista");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (pids[i] == pid)
    {
      pids[i] = 0;
      i++;
      break;
    }
  }
  // Una vez hemos encontrado la posición en la que estaba nuestro pid, desplazamos el resto
  for (; i < MAX_MINEROS; i++)
  {
    pids[i - 1] = pids[i];
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
  int *shared_target = NULL;

  fd_shm = shm_open(FICHERO_TARGET, O_RDWR, 0);
  if (fd_shm == -1)
  {
    perror(" Error creating the shared memory segment \n ");
    exit(EXIT_FAILURE);
  }

  shared_target = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (shared_target == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en new_target");
    exit(EXIT_FAILURE);
  }

  *shared_target = target;

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
  fd_shm = shm_open(FICHERO_TARGET, O_RDWR, 0);
  if (fd_shm == -1)
  {
    perror(" Error opening the shared memory segment \n ");
    close(fd_shm);
    exit(EXIT_FAILURE);
  }

  target = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (target == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en leer_target");
    exit(EXIT_FAILURE);
  }
  ret = *target;

  close(fd_shm);
  munmap(target, sizeof(int));
  /*En caso de que no se hubiese escrito un target todavía por un minero, el valor de inicialización es NO_TARGET*/
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
  int *pids = NULL;
  int i;

  if ((fd_shm = shm_open(FICHERO_PIDS, O_RDONLY, 0)) == -1)
  {
    perror(" shm_open en count_players");
    exit(EXIT_FAILURE);
  }
  pids = mmap(NULL, MAX_MINEROS * sizeof(int), PROT_READ, MAP_PRIVATE, fd_shm, 0);
  if (pids == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap");
    exit(EXIT_FAILURE);
  }

  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (pids[i] != 0)
    {
      players++;
    }
    else
    {
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
int wait_votation(sem_t *mutex_votacion, int corredores, int yesNo[2])
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

 

  // eliminamos a uno de lo corredores, sera el propio ganador, que no vota
  corredores--;
  while (i < MAX_INTENTOS && !votacionTerminada && !terminar)
  {
    votados = 0;

    // leemos cuantos han votado

    if ((fd_shm = shm_open(FICHERO_VOTACION, O_RDWR, 0)) == -1)
    {
      perror(" shm_open wait_votation ");
      exit(EXIT_FAILURE);
    }
    votations = mmap(NULL, MAX_MINEROS * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
    if (votations == MAP_FAILED)
    {
      close(fd_shm);
      perror("mmap en wait_votation");
      exit(EXIT_FAILURE);
    }
    close(fd_shm);

    if (wait_sem_interrumpible(mutex_votacion) == -1)
    {
      munmap(votations, MAX_MINEROS * sizeof(char));
      return -1;
    }
    for (j = 0; j < MAX_MINEROS; j++)
    {
      if (votations[j] != 0)
      {
        votados++;
      }
      else
      {
        break;
      }
    }
    
    munmap(votations, MAX_MINEROS * sizeof(char));
    sem_post(mutex_votacion);

    if (votados == corredores)
      votacionTerminada = 1;
    // esperamos 10  milisegundos a probar otra vez
    nanosleep(&ts, NULL);
    i++;
  }

  // una vez terminada la votacion verificamos si es correcta y damos moneda
  if ((fd_shm = shm_open(FICHERO_VOTACION, O_RDWR, 0)) == -1)
  {
    perror(" shm_open fichero votacion ");
    exit(EXIT_FAILURE);
  }
  votations = mmap(NULL, MAX_MINEROS * sizeof(char), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);
  if (votations == MAP_FAILED)
  {
    close(fd_shm);
    perror("mmap en wait_votation2");
    exit(EXIT_FAILURE);
  }
  if (wait_sem_interrumpible(mutex_votacion) == -1)
  {
    close(fd_shm);
    munmap(votations, MAX_MINEROS * sizeof(char));
    return -1;
  }
  for (i = 0; i < MAX_MINEROS; i++)
  {
    if (votations[i] != 0)
    {
      if (votations[i] == YES)
        yesNo[0]++;
      else
        yesNo[1]++;
    }
    else
    {
      break;
    }
  }

  // reseteamos las votaciones para la siguiente ronda truncando el fichero
  memset(votations, 0, MAX_MINEROS);

  close(fd_shm);
  munmap(votations, MAX_MINEROS * sizeof(char));
  sem_post(mutex_votacion);
  return 0;
}

/**
 * @brief Libera las estructuras que se han guardado durante la ejecucion del programa
 *
 * @param n_threads
 * @param arg_array
 * @param thread_array
 * @param mutex_pids
 * @param mutex_target
 * @param ganador_sem
 * @param mutex_votacion
 * @param mutex_wallets
 */
void clean_and_free(int n_threads, ArgsSolucion **arg_array,
                    pthread_t *thread_array, sem_t *mutex_pids, sem_t *mutex_target, sem_t *ganador_sem, sem_t *mutex_votacion, sem_t *mutex_wallets)
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
  sem_close(mutex_wallets);
}

/**
 * @brief Abre la cola de mensajes que comunica con el comprobador
 *
 * @return mqd_t la cola de mensajes creada
 */
mqd_t open_message_queue()
{
  mqd_t mqd;

  if ((mqd = mq_open(MINER_COMPROBADOR_MESSAGE_QUEUE, O_WRONLY)) ==
      (mqd_t)-1)
  {
    if (errno == ENOENT)
    {
      return MONITOR_DIED;
    }
    perror("mq_open");
    exit(EXIT_FAILURE);
  }
  return mqd;
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

/**
 * @brief Funcion con la que el minero configura todas las señales, sus máscaras y handlers
 *
 */
void configuracion_señales(sigset_t *origMask, sigset_t *block2mask, sigset_t *block1mask,
                           struct sigaction *act_alarm, struct sigaction *act_sigusr1, struct sigaction *act_sigusr2)
{

  /*Aplicamos la máscara que será empleada por los mineros*/
  /*Bloqueamos la señal 1, pues será la que podrán recibir los mineros que no lideren las carreras
    De esta manera nos aseguramos que la señal no se puede perder antes de que un proceso llegue a sigsuspend con la otra mascara*/
  sigemptyset(block1mask);
  sigaddset(block1mask, SIGUSR1);
  if (pthread_sigmask(SIG_BLOCK, block1mask, origMask) == -1)
  {
    perror("pthread_procmask");
  }
  /*Preparamos la máscara que desbloquea SIGUSR1 y bloquea SIGUSR2  para comenzar la carrera*/
  sigemptyset(block2mask);
  sigaddset(block2mask, SIGUSR2);

  // Configuramos la señal de alarma
  act_alarm->sa_handler = handler_alarm;
  sigemptyset(&(act_alarm->sa_mask));
  // No usamos SA_RESTART en SIGALRM: queremos que pueda interrumpir esperas bloqueantes.
  act_alarm->sa_flags = 0;

  if (sigaction(SIGALRM, act_alarm, NULL) < 0)
  {

    perror(" sigaction ");
    exit(EXIT_FAILURE);
  }

  // Configuramos el manejador para sigursr1
  act_sigusr1->sa_handler = handler_sigusr1;
  sigemptyset(&(act_sigusr1->sa_mask));
  act_sigusr1->sa_flags = SA_RESTART;

  if (sigaction(SIGUSR1, act_sigusr1, NULL) < 0)
  {
    perror(" sigaction ");
    exit(EXIT_FAILURE);
  }
  // Configuramos el manejador para sigursr2
  act_sigusr2->sa_handler = handler_sigusr2;
  sigemptyset(&(act_sigusr2->sa_mask));
  act_sigusr2->sa_flags = SA_RESTART;

  if (sigaction(SIGUSR2, act_sigusr2, NULL) < 0)
  {
    perror(" sigaction ");
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Funcion con la que el minero abre los semaforos que va a usar durante
 * su ejecucion.
 *
 * @param mutex_pids
 * @param mutex_target
 * @param ganador_sem
 * @param mutex_votacion
 * @param mutex_wallets
 * @param n_threads
 * @param arg_array
 * @param thread_array
 */
void configuracion_semaforos(sem_t **mutex_pids, sem_t **mutex_target, sem_t **ganador_sem, sem_t **mutex_votacion, sem_t **mutex_wallets,
                             int n_threads, ArgsSolucion **arg_array, pthread_t *thread_array)
{

  if ((*mutex_votacion = sem_open(MUTEX_VOTACION_SEM_NAME, 0, 0, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  if ((*mutex_wallets = sem_open(MUTEX_WALLETS, 0, 0, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    exit(EXIT_FAILURE);
  }

  if ((*mutex_pids = sem_open(MUTEX_PIDS_SEM_NAME, 0, 0, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    clean_and_free(n_threads, arg_array, thread_array, *mutex_pids, *mutex_target, *ganador_sem, *mutex_votacion, *mutex_wallets);
    exit(EXIT_FAILURE);
  }

  if ((*ganador_sem = sem_open(GANADOR_SEM, 0, 0, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    clean_and_free(n_threads, arg_array, thread_array, *mutex_pids, *mutex_target, *ganador_sem, *mutex_votacion, *mutex_wallets);
    exit(EXIT_FAILURE);
  }

  if ((*mutex_target = sem_open(MUTEX_TARGET_SEM_NAME, 0, 0, 1)) ==
      SEM_FAILED)
  {
    perror("sem_open");
    clean_and_free(n_threads, arg_array, thread_array, *mutex_pids, *mutex_target, *ganador_sem, *mutex_votacion, *mutex_wallets);
    exit(EXIT_FAILURE);
  }
}

int configuracion_mensajes(mqd_t *mqd, int minero_escribe[2], int registrador_escribe[2])
{
  *mqd = open_message_queue();
  if (*mqd == MONITOR_DIED)
  {
    return MONITOR_DIED;
  }

  /**Se cierran pipes pertinentes*/
  close(registrador_escribe[1]); /*registrador escribe (escritura)*/
  close(minero_escribe[0]);      /*minero escribe (lectura)*/
  return 0;
}

/**
 * @brief Funcion central del minado, incluye la ejecucion de cada ronda de la carrera
 * de minado.
 *
 * @param ganador
 * @param target
 * @param block2mask
 * @param n_threads
 * @param arg_array
 * @param thread_array
 * @param mqd
 * @param minero_escribe
 * @param registrador_escribe
 * @param mutex_target
 * @param ganador_sem
 * @param mutex_votacion
 * @param mutex_pids
 * @param mutex_wallets
 * @param yesNo
 */
void bucle_de_ejecucion(int *ganador, int target, sigset_t block2mask, int n_threads, ArgsSolucion **arg_array, pthread_t *thread_array,
                        mqd_t mqd, int minero_escribe[2], int registrador_escribe[2], sem_t *mutex_target, sem_t *ganador_sem, sem_t *mutex_votacion, sem_t *mutex_pids, sem_t *mutex_wallets,
                        int yesNo[2])
{
  struct timespec ts = {
      .tv_sec = 0,
      .tv_nsec = 10000000};
  int j, k;
  pthread_t h;
  int n_miners, error, ret_int;
  int rondas_ganadas = 0, rondas_verificadas = 0, rondas_corridas = 0;
  long solution;
  void *retval = NULL;
  char buffer[1024];
  char validado[] = "validated";
  char rejected[] = "rejected";
  char *status;
  char aux[MAX_MESSAGE];

  /*Ejecutamos el código del minero*/
  while (terminar == 0)
  {
    // Corremos una ronda más
    rondas_corridas++;
    // restauramos el valor de finished para la siguiente ronda
    finished = 0;

    if (*ganador)
    {
      // El proceso ganador (o el primero que llegue), espera a que haya mas jugadores esperando para correr

      if (wait_sem_interrumpible(mutex_pids) == -1)
        break;
      n_miners = count_players();
      sem_post(mutex_pids);
      while (n_miners < 2 && !terminar)
      {
        nanosleep(&ts, NULL);
        if (wait_sem_interrumpible(mutex_pids) == -1)
          break;
        n_miners = count_players();
        sem_post(mutex_pids);
      }
      // el primer proceso ha muerto en la espera de otros corredores
      if (terminar)
        continue;

      // empezamos al carrera

      if (wait_sem_interrumpible(mutex_pids) == -1)
        break;
      start_race(getpid());
      sem_post(mutex_pids);
    }
    else
    {
      /*Esperamos a recibir la señal de comienzo de la carrera*/
      /*Aplicamos la máscara que bloqueará la señal dos, que es la que los mineros que pierdan aplicarán posteriormente*/
      /*Le pasamos así una máscara en la que no aparece SIGUSR1 para que el prceso se desbloquee con dicha señal*/

      sigsuspend(&block2mask);
      if (terminar == 1)
        continue;
    }

    /******************************/
    /**********CARRERA*************/
    /******************************/

    // Nos guardamos el numero de mineros que compiten en esta ronda, para esperar en la votacion

    if (wait_sem_interrumpible(mutex_pids) == -1)
      break;
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
        mq_close(mqd);
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
    if (wait_sem_interrumpible(mutex_target) == -1)
      break;
    ret_int = sem_trywait(ganador_sem);
    if (ret_int == 0)
    {

      // PROCESO GANADOR, MANDA SEÑAL A LOS PERDEDORES Y ESPERA A QUE VOTEN
      // mandamos señal a todos los demas procesos de terminar la carrera
      if (wait_sem_interrumpible(mutex_pids) == -1)
      {
        sem_post(mutex_target);
        sem_post(ganador_sem);
        break;
      }
      start_votation(getpid());
      sem_post(mutex_pids);

      new_target(solution);
      sem_post(mutex_target);
      /*Esperamos a que todos los procesos voten*/
      if (wait_votation(mutex_votacion, n_miners, yesNo) == -1)
      {
        sem_post(ganador_sem);
        break;
      }
      // Sumamos a las rondas ganadas del minero y si gana la votacion sumamos a las rondas verificadas
      rondas_ganadas++;
      if (yesNo[0] >= yesNo[1])
      {
        rondas_verificadas++;
        status = validado;

        // sumamos moneda en el registro del sistema
        if (wait_sem_interrumpible(mutex_wallets) == -1)
        {
          sem_post(ganador_sem);
          break;
        }
        sumarMonedaWallets(getpid());
        sem_post(mutex_wallets);
      }
      else
      {
        status = rejected;
      }
      *ganador = 1;
      // Imprimimos por terminal
      printf("Winner <%d> Yes|No: (%d|%d) => %s\n", getpid(), yesNo[0], yesNo[1], yesNo[0] >= yesNo[1] ? "Accepted" : "Rejected");
      // Mandamos al registrador también la información
      sprintf(buffer, "%d|%d|%ld|%d|%d|%d|%s\n", rondas_corridas, target, solution, yesNo[0], yesNo[1], rondas_verificadas, status);
      write(minero_escribe[1], buffer, strlen(buffer) + 1);

      // Enviamos la solución al comprobador
      sprintf(aux, "%d %ld", target, solution);

      // bucle de comprobación, se asegura no ser interrumpido por señales
      while ((mq_send(mqd, aux, MAX_MESSAGE, 0)) == (mqd_t)-1)
      {
        if (errno == EINTR)
        {
          if (terminar)
          {
            sem_post(ganador_sem);
            break;
          }
          continue;
        }

        perror("mq_send");
        exit(EXIT_FAILURE);
      }

      if (terminar)
        break;

      // Escribimos el nuevo valor para el target
      target = solution;

      /**Leemos la señal del registrador para continuar con la siguiente ronda
       */

      while (read(registrador_escribe[0], buffer, sizeof(buffer)) <= 0)
      {
        if (errno == EINTR && terminar == 0)
          continue; // fue otra señal, reintentar la lectura del pipe con el registrador
        // fue SIGALRM (terminar==1) o un error real, entonces salimos

        close(minero_escribe[1]);
        close(registrador_escribe[0]);
        if (wait_sem_interrumpible(mutex_pids) == 0)
        {
          desinscribirseLista(getpid());
          n_miners = count_players();
          sem_post(mutex_pids);
        }
        sem_post(ganador_sem);
        mq_close(mqd);
        clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion, mutex_wallets);
        exit(EXIT_SUCCESS);
      }

      sem_post(ganador_sem);
    }
    else
    {
      // PROCESO PERDEDOR, ESPERA A CAMBIO DE TARGET Y VOTA
      // leemos el nuevo target y hacemos votacion
      solution = leer_target();
      sem_post(mutex_target);
      if (wait_sem_interrumpible(mutex_votacion) == -1)
        break;
      votar(target, solution);
      sem_post(mutex_votacion);
      // Consta que no es el ganador para empezar la siguiente ronda
      target = solution;
      *ganador = 0;
    }
  }
}

void funcionalidad_minero(int minero_escribe[2], int registrador_escribe[2], int n_secs, int n_threads)
{
  long interval = 0;
  int i;
  int yesNo[2] = {0, 0};
  sem_t *mutex_pids = NULL;
  sem_t *mutex_target = NULL;
  sem_t *ganador_sem = NULL;
  sem_t *mutex_votacion = NULL;
  sem_t *mutex_wallets = NULL;

  sigset_t origMask, block2mask, block1mask;
  int n_miners;
  int ganador = 0;
  int target;
  char aux[MAX_MESSAGE];
  mqd_t mqd;
  struct sigaction act_alarm, act_sigusr1, act_sigusr2;
  int monitor_died = 0;

  pthread_t *thread_array = NULL;
  ArgsSolucion **arg_array = NULL;

  // Semilla para el numero aleatorio utilizado en las votaciones
  srand(time(NULL));

  /*************************************************
   **************CONFIGURACIONES PREVIAS************
   **************************************************/

  // Configuramos mensajes, la cola y pipe con el registrador
  monitor_died = configuracion_mensajes(&mqd, minero_escribe, registrador_escribe);
  if (monitor_died)
  {
    printf("No hay ningún comprobador para empezar la carrera :(\n");
    exit(EXIT_SUCCESS);
  }

  // Configuramos los handlers para las señales y las mascaras
  configuracion_señales(&origMask, &block2mask, &block1mask, &act_alarm, &act_sigusr1, &act_sigusr2);

  /**Abrimos todos los semaforos para la ejecucion de las tareas coordinadas */

  configuracion_semaforos(&mutex_pids, &mutex_target, &ganador_sem, &mutex_votacion, &mutex_wallets, n_threads, arg_array, thread_array);

  // Iniciamos la cuenta con la alarma
  alarm(n_secs);

  /*Accedemos al fichero de los pids*/
  if (wait_sem_interrumpible(mutex_pids) == -1)
  {
    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    mq_close(mqd);
    clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion, mutex_wallets);
    exit(EXIT_SUCCESS);
  }
  inscribirseLista(getpid());
  sem_post(mutex_pids);

  if (wait_sem_interrumpible(mutex_wallets) == -1)
  {
    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    mq_close(mqd);
    clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion, mutex_wallets);
    exit(EXIT_SUCCESS);
  }
  inscribirseWallets(getpid());
  sem_post(mutex_wallets);

  /*******************************/
  /****RESERVAR MEMORIA DE HILOS**/
  /*******************************/

  /*Calculamos el intervalo de valores que recorrerá cada hilo*/
  interval = (POW_LIMIT - 1) / n_threads + 1;

  /*Reservamos la memoria necesaria para el programa*/
  arg_array = (ArgsSolucion **)malloc(sizeof(ArgsSolucion *) * n_threads);
  thread_array = (pthread_t *)malloc(sizeof(pthread_t) * n_threads);

  if (!arg_array || !thread_array)
  {
    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    if (wait_sem_interrumpible(mutex_pids) == 0)
    {
      desinscribirseLista(getpid());
      sem_post(mutex_pids);
    }
    clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion, mutex_wallets);
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
      if (wait_sem_interrumpible(mutex_pids) == 0)
      {
        desinscribirseLista(getpid());
        sem_post(mutex_pids);
      }
      clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion, mutex_wallets);
      printf("miner <%d> exited with status 1\n", getpid());
      wait(NULL);
      exit(EXIT_FAILURE);
    }
    arg_array[i]->n_valores = interval;
    arg_array[i]->n_hilo = i;
  }

  /*Accedemos al fichero de los targets*/

  if (wait_sem_interrumpible(mutex_target) == -1)
  {
    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    mq_close(mqd);
    clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion, mutex_wallets);
    exit(EXIT_SUCCESS);
  }
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

  bucle_de_ejecucion(&ganador, target, block2mask, n_threads, arg_array, thread_array, mqd, minero_escribe,
                     registrador_escribe, mutex_target, ganador_sem, mutex_votacion, mutex_pids, mutex_wallets, yesNo);

  /**********FINAL DE EJECUCION Y MUERTE DEL PROCESO**************/

  // Si el proceso que sale es el ganador, procede a mandarle la señal de comienzo de la carrera a los demas corredores
  // esto para evitar bloqueos
  if (ganador == 1)
  {
    if (wait_sem_interrumpible(mutex_pids) == 0)
    {
      start_race(getpid());
      desinscribirseLista(getpid());
      n_miners = count_players();
      sem_post(mutex_pids);
    }
    else
    {
      n_miners = 1;
    }
  }
  else
  {
    if (wait_sem_interrumpible(mutex_pids) == 0)
    {
      desinscribirseLista(getpid());
      n_miners = count_players();
      sem_post(mutex_pids);
    }
    else
    {
      n_miners = 1;
    }
  }
  if (n_miners == 0)
  {
    sprintf(aux, MINERS_ENDED);

    mq_send(mqd, aux, MAX_MESSAGE, 0);
  }
  // Vemos que la seccion critica unifica desinscribirse y countplayers. Esto soluciona el caso de que dos se desapunten y lean 0 personas a la vez
  // Cuando el primero en desapuntarse deberia haber leido que quedaba uno

  // limpiamos ejecucion
  clean_and_free(n_threads, arg_array, thread_array, mutex_pids, mutex_target, ganador_sem, mutex_votacion, mutex_wallets);
  mq_close(mqd);
}