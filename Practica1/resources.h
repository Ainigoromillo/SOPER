/**
 *@file resources.h
 *@author Matteo Artuñedo
 *@version 0.1
 *@date 2026-05-03
 *Fichero que guarda los nombres de los recursos (memoria compartida y semáforos) a los que accederán los distintos programas
 */
#ifndef PRACTICA1_RESOURCES_H
#define PRACTICA1_RESOURCES_H
/*Includes*/
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
#include <mqueue.h>
#include "errno.h"

/*SEMÁFOROS*/
#define MUTEX_PIDS_SEM_NAME "/mutex_pids_sem"
#define MUTEX_TARGET_SEM_NAME "/mutex_target"
#define GANADOR_SEM "/ganador_sem"
#define MUTEX_VOTACION_SEM_NAME "/mutex_voting"

#define MINER_COMPROBADOR_MESSAGE_QUEUE "/miner_comprobador_mq"
#define COMPROBADOR_MONITOR_MESSAGE_QUEUE "/comprobador_monitor_mq"

#define MAX_MINEROS 20
#define NO_TARGET -1
#define MAX_MESSAGE 1024

/*MEMORIA COMPARTIDA*/
#define FICHERO_PIDS                                                        \
"Pids.pid" /**Nombre del fichero donde se guardarán los pids de los mineros \
que participen en las carreras*/

#define FICHERO_TARGET                                                         \
"Target.tgt" /**Nombre del fichero donde se esciribrá el target que usarán \
los mineros que participen en las carreras*/

#define FICHERO_VOTACION                                                      \
"Voting.vot" /**Nombre del fichero donde los procesos perdedores apuntan su \
votacion y el proceso ganador comprueba si todos han votado */

#define MINERS_ENDED "MinersEnded"

#endif //PRACTICA1_RESOURCES_H