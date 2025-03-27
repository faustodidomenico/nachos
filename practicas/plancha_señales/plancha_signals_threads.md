# Práctica planificación y señales.

### Alumno: Federico Stizza

1) 

* Abstracción: un ejemplo de esto es la idea de que la información se organiza en archivos que se encuentran y diferentes directorios (los cuales se pueden encontrar en uno o mas dispositivos de almacenamientos).
* Aislamiento: un ejemplo de esta propiedad en los sistemas operativos es el espacio de memoria virtual, esto produce que cada proceso tenga su propio espacio (de manera figurativa) y no interfiera con las instrucciones de otro.
* Administración de recursos: El planificador es un ejemplo de la administración de recursos, ya que el que se encarga de repartir los recursos entre los procesos que están compitiendo por ellos. El planificador se encarga de optimizar los recursos y maximizar el rendimiento de los procesos.

2) SIG_IGN y SIG_DFL son dos manejadores por defecto de señales, los cuales ignoran la señal y utilizan la función manejadora por defecto asociada a la señal respectivamente. El valor de retorno de la función 'signal' es el manejador anterior asociado a la señal, útil para reestablecerlo.

3) 

* Verdadero, si un proceso tiene otro proceso hijo (en estado listo) y el proceso padre tiene que abortar su ejecución entonces el proceso hijo pasa de listo a zombie/terminado.
* Falso, los procesos también pueden bloquearse por mecanismos de concurrencia como los locks, semaphore, etc.
* Falso, un proceso que se ejecute en un intervalo en el que no se realicen llamadas al sistema o bloqueos de concurrencia termina sin pasar por el estado de bloqueo.
* Falso, ya que los programas interactivos son los procesos que esperan por eventos producidos por el usuario. Estos procesos tardarían mas en responder a dichos eventos ya que si el evento no se produjo durante el quantum de ejecución este proceso pasa al final de la ronda teniendo que esperar n quantums (cantidad de procesos de la ronda) para poder responder al evento producido.

4) 

C -  C -  C - C - C  -  C  -  C  -  C
1     2     3    4  13    14    15   16
       A  - A
      17   18
                            B - B - B - B
                            5    6    7   8
                                    D -  D  - D  - D
                                    9    10   11   12
0  -  1  -  2  -  3  -  4  -  5  -  6  -  7  -  8

Por lo tanto, despreciando todo lo mencionado en el enunciado, los tiempos de respuesta T son:

* T(A) = 17
* T(B) = 4
* T(C) = 16
* T(D) = 7

Luego del cambio del contexto dado por la llegada del proceso B, supongo que el proceso C se vuelve a encolar en la cola de prioridad 5, por lo que queda listo a ejecutarse luego de D.

5) Las ventajas de usar 'sigaction' en lugar de las tradicionales 'signal' de UNIX son:

* Implementa la estructura siginfo_t que proporciona mas información sobre la señal que se produjo y otros datos relacionados de utilidad, como quien lo origino, si hubo fallo, etc.
* Además proporciona diferentes mecanismos para resolver los problemas de concurrencia, se puede especificar que hacer cuando llegan dos señales al mismo tiempo.
* Se pueden desarrollar manejadores de señales más potentes, que pueden determinar que hacer durante una llamada del sistema, que hacer con el nuevo manejador, con el anterior, etc. 