## Plancha de Sistemas Distribuídos y Microkernels.

## Stizza, Federico.

### 1)

Sean t1 y t2 los dos procesos que intentan acceder a la región crítica r.

Entonces, t1 envía <t1,r,h1> y t2 <t2,r,h2>. h1,h2 son los timestamps de inicio de la solicitud, únicos por hipótesis por lo que h1 != h2.

Las solicitudes se le envían al resto de los procesos que estén en la "red", los que no estén accediendo o no requieran acceder responderan OK.

Suponiendo que el resto de los procesos no deseen acceder a la región crítica r,dependiendo de cual de los dos timestamps es menor, el proceso con solicitud asociada a ese timestamp recibirá el OK y entrará a región crítica mientras que el otro proceso esperará que termine. Cuando termina le contestará OK al otro proceso que quedó esperando y por último podrá acceder.

### 2)

Bajo los supuestos planteados en el enunciado, la única diferencia a nivel de procesamiento sería que el sistema de un solo nodo con muchos procesadores tiene la ventaja de que comparten los recursos del nodo, es decir, comparten memoria, hardware, etc.

El procesamiento si bien es distribuído entre las CPU, pierde las otras ventajas brindadas por el sistema físicamente distribuído. Menor latencia de la red al acceder al nodo más cercano, redundancia y seguridad, entre otras.

Como se mencionó al principio al compartir recursos existen menos problemas de sincronización ya que todo se puede resolver con los mecanismos primitivos de sincronización (locks, semáforos y condiciones).

### 3)

El kernel de NachOS es de tipo monolítico ya que si bien cada aspecto del núcleo está separado en módulos, estos no pueden cargarse dinámicamente ya que no corren como procesos de usuario. El kernel implementa los rasgos básicos de un sistema, gestión de procesos, de memoria, abstracción del sistema de archivos y red.

Los procesos de usuario se comunican con estas características a través de llamadas del sistema. 

Si se quisiera migrar a una estructura de microkernel se deberían modificar las abstracciones del sistema de archivos, de gestión de red entre otros para que corran como procesos de usuario. 
