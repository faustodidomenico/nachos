# Plancha Memoria 2 - SOII

## Autor: Stizza, Federico.

## 1)

En un sistema que tiene `Copy on Write`, las paginacion del proceso padre e hijo inicialmente se encuentran marcadas
como `compartidas` por ambos. Luego, cuando el proceso hijo llama a `execve`, seguiran compartiendo una buena parte
de las paginas, en  particular las que refieran a librerias del sistema. Pero luego el hijo  y este
traera a memoria ciertas especificas que solo utilizara este proceso para mostrar los directorios y archivos por consola.

#GRAFICO AQUI

## 2) 

Se utilizan 5 paginas fisicas y politica FIFO para el reemplazo de las paginas.
