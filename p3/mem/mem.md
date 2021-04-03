author:
- Pablo Cuesta Sierra
geometry:
- 2.5cm
---

# Memoria compartida

## Ejercicio 1

a) Primero (línea 2), se intenta crear un segmento de memoria compartida, con las opciones seleccionadas de tal manera que si existe, `shm_open` devuelva error ($-1$). Después (línea 3), se trata este error y, en caso de que el error se deba a que el segmento con ese nombre ya existía, se abre este segmento (esta vez sin usar la flag de crear). Si el error no era porque ya existiera o si al intentar abrir el se
gmento ya existente hay otro error, se manda una cadena por *stderr*.

