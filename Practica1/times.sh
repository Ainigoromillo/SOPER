#!/bin/bash

# Archivo donde se guardarán los resultados
OUTPUT="time.log"

# Limpiar archivo antes de empezar
echo "# n_hilos tiempo" > $OUTPUT

for i in {1..200}
do
    suma=0
    media=0
    for j in {1..5}
    do
    
    # Ejecutamos el programa y capturamos solo el tiempo real
    #redirigimos stderr a stdout y stdout a dev/tty para seguir viendo la salida de ejecucion de miner, el time lo mete en stderr y de ahi pasa a stdout para que tiempo lo recoja
    tiempo=$( /usr/bin/time -f "%e" ./miner 10 50 $i 2>&1 >/dev/tty )
    suma=$(echo "$suma + $tiempo" | bc -l)
    done 
    # Guardamos argumento y tiempo en el archivo
    media=$(echo "$suma / 5" | bc -l)
    echo "$i $media" >> $OUTPUT

done

gnuplot -persist -e "set title 'Tiempo vs n_hilos'; \
set xlabel 'Hilos'; \
set ylabel 'Tiempo (s)'; \
plot 'time.log' using 1:2 with linespoints"

rm *.log