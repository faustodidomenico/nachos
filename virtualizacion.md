## Plancha Virtualización 1

### Stizza, Federico.

### 1)

|             | Hipervisor | Emulador | Paravirtualizacion |
| ----------- | ---------- | -------- | ------------------ |
| Ventajas    | Ligeros ya que se ejecutan como procesos de usuario. | Si bien es un concepto más complejo, puede ser hasta más eficiente que los hipervisores debido al cacheo de las instrucciones. | El estándar propuesto hace más claro y sencillo el desarrollo de los hipervisores en términos del SO anfitrión ya solo se deben consumir las APIs provistas.|
| Desventajas | Depende del SO anfitrión para realizar las instrucciones sensibles.| Debe crear bloques de instrucciones en tiempo de ejecución y emularlas en modo usuario.| El hipervisor debe adaptarse a estas APIs. |

### 2) 

Una *shadow* page table es una tabla de paginación que es utilizada para resolver la traduccion de páginas físicas de un sistema operativo *huésped* a las páginas físicas del sistema anfitrión.

Generalmente es implementada a nivel de hardware. Sin este soporte se pierde la abstracción generada por la virtualización ya que el sistema debería saber que está siendo virtualizado y debe resolver las direcciones físicas para poder funcionar. En síntesis sin este soporte el sistema huésped no puede resolver sus direcciones físicas.

### 3)

a- Virtualización asistida por hardware.
Ejecutar en simultáneo dos sistemas operativos es muy costoso y este método, aunque es más costoso ya que requiere apoyo del hardware, genera mayor fluidez. Además esta situación es muypropensa a tener mucho flujo de I/O por lo que la separación de la memoria para las operaciones de entrada salida también brinda mayor seguridad en la virtualización y eficiencia.

b- La mayoría de los dispositivos móviles trabajan sobre arquitecturas ARM, por lo que para *testear* las aplicaciones desarrolladas se deben emular las instrucciones del dispositivo, ademáslas aplicaciones en modo *debug* generalmente son muy pesadas por lo que la eficiencia de los emuladores es vital.

c- Contenedores. Todos los servidores web por defecto utilizan un puerto específico, por lo que utilizar contenedores le brinda la abstracción suficiente para poder trabajar sin emular unsistema operativo completo.