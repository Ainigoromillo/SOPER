#include "resources.h"

#define _POSIX_C_SOURCE 200809L
#define MAX_BUFFER 4
#define MAX_INTENTOS 100 // El número maximo de esperas que hace el proceso ganador a que los demas voten
#define YES 'y'
#define NO 'n'

void funcionalidad_minero(int minero_escribe[2], int registrador_escribe[2], int n_secs, int n_threads);