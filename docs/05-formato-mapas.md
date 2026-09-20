# 05 · Formato de mapas, versión 1

Studio escribe el formato v2, descrito en [formatos de proyecto y mapa
v2](11-formato-v2.md). Añade IDs estables, barreras y volúmenes superpuestos sin
romper la carga de estos mapas v1.

## Reglas generales

Texto UTF-8 sin BOM, números con punto decimal, comentarios con `#` y una
directiva por línea. La primera directiva debe ser `retro_map 1`. Las líneas
vacías y comentarios se ignoran. Máximo: 1 MiB por archivo y 1023 bytes por línea.

No hay punteros, rutas de recursos ni instrucciones ejecutables dentro del mapa.
Los materiales son índices de la paleta que proporciona la aplicación.

```text
retro_map 1
sector ID FLOOR CEILING LIGHT WALL_MATERIAL FLOOR_MATERIAL CEILING_MATERIAL
v X Y
v X Y
v X Y
link EDGE NEIGHBOR_SECTOR NEIGHBOR_EDGE
spawn KIND SECTOR X Y Z YAW_DEGREES
```

`v` y `link` se refieren al último `sector`. `spawn` es independiente del sector
actual. Los IDs de sector deben aparecer consecutivamente desde cero.

| Campo | Contrato |
|---|---|
| Coordenadas y alturas | Finitas; valor absoluto máximo 10 000. Metros. |
| Sector | Entre 3 y 16 vértices, estrictamente convexo, orden antihorario. |
| Suelo/techo | Techo al menos 0.1 m sobre el suelo al cargar. |
| Luz | Entre 0 y 1 inclusive. |
| Material | Entero entre 0 y 15. La aplicación provee esa tabla. |
| Conexión | Índices válidos, extremos coincidentes y relación recíproca. |
| Marcador | Etiqueta de hasta 23 bytes, posición dentro de su sector. |
| Cantidad total | Máximo 64 sectores y 128 marcadores. |

Los polígonos no pueden solapar sus interiores, aunque tengan alturas distintas.
No se admite habitación sobre habitación. Los segmentos compartidos deben tener
extremos iguales en orden inverso. La validación SAT distingue tocar de solapar.

## Una habitación

```text
retro_map 1
sector 0 0 3 1 0 1 2
v 0 0
v 6 0
v 6 6
v 0 6
spawn camera 0 3 2 0 0
```

Las aristas tienen índices 0→1, 1→2, 2→3 y 3→0. Al caminar por los vértices
en ese orden, el interior queda a tu izquierda.

## Conectar habitaciones

El `assets/lab.map` conecta la arista este del sector 0 con la oeste del sector 1:

```text
# Dentro del sector 0
link 1 1 3
# Dentro del sector 1
link 3 0 1
```

Ambas aristas van de `(6,0)` a `(6,6)` en sentidos opuestos. No basta con que
una arista toque un fragmento de otra. Para una puerta más estrecha que una sala,
descompón la planta en sectores convexos adicionales; no añadas vértices
colineales a un polígono que debe ser estrictamente convexo.

`floor` diferente produce un escalón; `ceiling` diferente produce un dintel.
No todo portal visible es transitable por un cuerpo alto. El laboratorio sirve
para observar esta diferencia.

## Marcadores del motor y del FPS

El núcleo almacena `kind` como una etiqueta sin interpretar. El laboratorio usa
un marcador de cámara. Foundry acepta:

| Tipo | Significado |
|---|---|
| `player` | Un único inicio de jugador. |
| `guard` | Guardia, máximo 16. |
| `health` | Botiquín de 35, hasta vida máxima 100. |
| `ammo` | Caja de 18 proyectiles. |
| `key` | Una única llave de la compuerta. |
| `door` | Un sector vacío cuyo techo comienza cerrado y sube al interactuar. |
| `exit` | Una única terminal; acercarse finaliza el nivel. |

Foundry exige exactamente un `player`, `door`, `key` y `exit`, y máximo 32
pickups incluidos llave y salida. Los actores deben aparecer en el suelo,
caber de pie y mantener margen contra las paredes sólidas. El sector de puerta
no puede contener otros marcadores.

La validación estructural no demuestra que tu puzzle pueda completarse: colocar
la llave detrás de su puerta sería un error de diseño. Prueba el recorrido real.

## Paleta de Foundry

0: metal gris; 1: suelo en losetas; 2: techo con luminarias; 3: panel naranja;
4: compuerta; 5: terminales verdes. 6–15 tienen material metálico generado y
están disponibles para experimentar. La paleta del laboratorio es distinta:
los índices pertenecen a la aplicación, no a una interpretación fija del motor.

## Cambiar tu primer nivel

1. Copia `assets/foundry.map` con otro nombre.
2. Cambia la luz del depósito de `1` a `0.6` y su material de pared de `3` a `0`.
3. Mueve la munición dentro del mismo sector; no cambies aún las conexiones.
4. Ejecuta `retro_fps --map ruta/a/tu.map`.
5. Luego cambia el suelo del depósito a `0.5`: el jugador ya no subirá andando
   con su paso de 0.3 m; necesitará saltar. Comprueba también que el guardia no
   recibe una ruta que requiera subir un escalón que su configuración no admite.

Si un archivo falla, la aplicación muestra ruta, línea y motivo en la consola.
No reemplaza los datos inválidos por un mapa oculto ni continúa con índices rotos.
