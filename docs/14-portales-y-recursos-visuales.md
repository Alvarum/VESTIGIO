# Portales parciales y recursos visuales

Este capitulo describe dos contratos que evitan los defectos visuales mas
costosos de diagnosticar: paredes que desaparecen al conectar habitaciones y
hojas de sprites que se muestran completas.

## Una conexion no elimina una pared

Desde `retro_map 4`, cada directiva `link` incluye el intervalo horizontal de
la abertura:

```text
link <arista> <sector_vecino> <arista_vecina> <inicio> <fin>
```

`inicio` y `fin` pertenecen a `[0, 1]` y avanzan desde el primer vertice de la
arista hasta el segundo. Por ejemplo, `0.2 0.8` conserva un 20 % de pared a
cada lado. La cara reciproca recorre el muro en sentido contrario, por lo que
debe almacenar `[1-fin, 1-inicio]`.

El renderer divide la arista en jambas laterales, panel inferior, dintel y
abertura. Colision, rayos, percepcion y navegacion consultan el mismo intervalo.
Una puerta o ventana cubre solo la abertura asociada. Esta fuente unica evita
que un hueco se vea abierto pero siga bloqueando, o que la IA atraviese una
parte visualmente solida.

Los mapas v1-v3 siguen cargando. Sus conexiones se migran en memoria a `[0,1]`
porque ese era su significado historico. Studio escribe v4; guardar una copia
no modifica el archivo anterior hasta que el usuario ejecuta Guardar.

## Atlas de personajes

Un archivo `retro_actor 1` declara `sprite`, `cell_width` y `cell_height`. Cada
frame de animacion contiene un indice de celda. La cantidad de columnas es:

```text
columnas = ancho_textura / ancho_celda
```

La fila y columna se calculan con division y modulo. El renderer convierte el
rectangulo de esa celda a UV normalizadas y nunca usa el atlas completo como
recuperacion de error. Una celda ausente usa la celda cero; dimensiones
incompatibles impiden dibujar el recurso y deben aparecer como error de
proyecto en Studio.

NPC amistosos y enemigos usan este mismo camino. Sus diferencias pertenecen a
gameplay, no al renderer. Los pickups usan sprites recortados independientes,
por lo que tampoco reutilizan una textura de pared como cartel rectangular.

## Orden recomendado para depurar

1. Activar la vista de portales y comprobar el intervalo sobre ambas caras.
2. Lanzar un rayo por el centro de la abertura y otro contra una jamba.
3. Comprobar movimiento con el radio real del cuerpo, especialmente en bordes.
4. Validar que el atlas sea multiplo exacto del tamaño de celda.
5. Revisar celda, direccion y frame de la animacion activa.
6. Capturar la escena a 480 x 270 y examinar profundidad y oclusion.

Las pruebas `partial_portals` y `renderer_contracts` cubren los contratos
geometricos. La revision visual sigue siendo obligatoria porque una imagen
tecnicamente valida puede tener mala composicion, escala o legibilidad.
