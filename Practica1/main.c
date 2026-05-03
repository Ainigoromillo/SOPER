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
#include "resources.h"

/* Expose POSIX APIs such as sigprocmask and SIG_BLOCK */


#include "registrador.h"
#include "minero.h"

int main(int argc, char *argv[])
{
  long pid = 0;
  int n_secs = 0;
  int n_threads = 0;
    // Descriptores para el pipe del registrador
  int minero_escribe[2];
  int registrador_escribe[2];






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
    
    
    close(minero_escribe[1]);
    close(registrador_escribe[0]);
    funcionaldadRegistrador(minero_escribe, registrador_escribe);
    close(minero_escribe[0]);
    close(registrador_escribe[1]);
    printf("Register of %d exited with status 0\n", getppid());
    exit(EXIT_SUCCESS);
  }
  /*Proceso padre: minero*/
  else
  {

    funcionalidad_minero(minero_escribe, registrador_escribe, n_secs, n_threads);
    return EXIT_SUCCESS;
  }
}
