# 11 · Formatos de proyecto v2, mapa v4 y actores

Todos los archivos admiten líneas en blanco y comentarios desde `#`. Los números
se validan como finitos; un error incluye línea cuando procede. Las cargas son
transaccionales: el objeto anterior permanece intacto si falla una directiva.

## Proyecto

```text
retro_project 2
id haunted_house
name Casa_de_la_niebla
rules horror_fps
death checkpoint
level levels/house.map
title_art art/haunted-title.png
logic logic/haunted.rules
dialogue dialogues/haunted.dialogue
actor actors/kidnapper.actor
```

Las rutas son relativas al manifiesto. Se rechazan rutas absolutas y componentes
`..` para impedir que un proyecto escriba fuera de su raíz.

`id` identifica guardados; sólo admite ASCII alfanumérico, `-` y `_`. `death`
acepta `restart_level`, `checkpoint`, `limited_lives` o `permadeath`. El motor
sigue importando `retro_project 1` en memoria; guardar desde Studio produce v2.

## Mapa v4

Los sectores y enlaces conservan la sintaxis v1. La diferencia esencial es que
dos polígonos pueden compartir XY si sus intervalos `[floor, ceiling)` no se
interpenetran.

```text
link EDGE NEIGHBOR NEIGHBOR_EDGE START END
spawn ID KIND DEFINITION SECTOR X Y Z YAW_DEGREES
barrier ID TYPE SECTOR EDGE MATERIAL BLOCKS HEALTH OPEN
```

`START` y `END` estan normalizados en `[0,1]`. La conexion reciproca usa
`[1-END, 1-START]` porque la arista compartida tiene sentido opuesto. Una
barrera cubre solo ese intervalo, no el muro completo. Los mapas v1-v3 se
cargan como aberturas de ancho completo para conservar su significado.

`BLOCKS` es una máscara: movimiento=1, visión=2, proyectil=4. Una puerta habitual
usa 7. Un vidrio usa 5: se ve a través de él, pero detiene cuerpos y balas hasta
que su vida llega a cero. `OPEN` está en `[0,1]`.

Un mapa v1/v2 se sigue cargando. El cargador genera IDs deterministas para sus
marcadores. Al guardarlo desde Studio se escribe v4 mediante un archivo temporal
y reemplazo atómico; el destino sólo cambia cuando el nuevo archivo está cerrado.

Triggers, luces y reglas viven en `retro_rules 1` para que una escena geométrica
pueda reutilizarse con otra lógica. Consulta el capítulo 12 para su gramática.

## Personaje

```text
retro_actor 1
id kidnapper
sprite art/kidnapper.png 32 48
body 0.32 1.72 0.30
movement 2.85 8.0
health 100 0
flags true true true true
tracking perception
perception 18 105 9 4
ranges 0.68 1.10

animation chase 8 true
frame 4 0.11 sound
frame 5 0.11 none

phase wrath 0.5 omniscient projectile 1.35 1.2 0
```

`flags` significa: invulnerable, aturdible, abre puertas y captura termina la
partida. Los ángulos del archivo están en grados; la API almacena radianes. Los
umbrales de fase van de mayor a menor y expresan una fracción de la vida máxima.

## Límites deliberados

Un proyecto admite 64 sectores, 128 marcadores, 128 barreras, 32 definiciones,
128 entidades vivas, 128 reglas, 64 triggers y 32 luces. Los límites convierten
desbordamientos en errores claros y evitan asignaciones impredecibles por frame.
