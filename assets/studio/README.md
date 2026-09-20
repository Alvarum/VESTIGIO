# Haunted: Casa de la Niebla

## Dirección visual

Los personajes y materiales de este ejemplo fueron creados para RetroForge
con el generador de imágenes de OpenAI y preparados como recursos locales PNG.
Las cuatro hojas de personajes tienen cuatro poses recortadas y transparencia;
los siete materiales comparten una paleta industrial de carbón, óxido, verde y
ámbar. No dependen de servicios externos durante la ejecución.

El juego carga exclusivamente los archivos `*-sheet.png` declarados por los
actores y los PNG de `art/materials` declarados por `haunted.retro`; las poses
anteriores de un solo fotograma fueron sustituidas para evitar recursos muertos
en el juego exportado.

Haunted es el showcase jugable de RetroForge. Su recorrido estÃ¡ diseÃ±ado para
demostrar sistemas reales del motor en una partida corta y legible, no para
ser una galerÃ­a de objetos aislados.

## Recorrido

1. Hablar con Elena en el refugio y activar el tablero elÃ©ctrico.
2. Romper el vidrio del patio con dos disparos.
3. Derrotar al cuidador y recoger la llave que deja caer una sola vez.
4. Usar la llave en el panel del ala de servicio y subir la escalera.
5. Entrar al circuito superior, romper la lÃ­nea de visiÃ³n de la secuestradora
   y alcanzar la sala verde por cualquiera de sus dos rutas.
6. Activar el panel, cruzar la pasarela segura y combatir al GuardiÃ¡n.
7. Volver a la salida verde despuÃ©s de derrotarlo.

Los checkpoints estÃ¡n en el vestÃ­bulo, antes de la persecuciÃ³n y al cerrar las
compuertas de seguridad. Una captura restaura el Ãºltimo estado completo.

## Sistemas demostrados

- Portales parciales, paredes conservadas alrededor de aberturas y colisiÃ³n
  compartida entre render, movimiento, visiÃ³n y disparos.
- Puerta abierta, ventana rompible, puerta con llave y compuertas controladas
  por reglas.
- Habitaciones con distinta altura y escalera construida con volÃºmenes 2.5D.
- NPC, conversaciÃ³n ramificada, variable global y objetivo visible.
- Pickup colocado, curaciÃ³n, drop al morir e inventario consumible.
- Perseguidora invulnerable, percepciÃ³n, navegaciÃ³n y captura con retry.
- Jefe de dos fases: invocaciÃ³n limitada y proyectiles fÃ­sicos bloqueados por
  la geometrÃ­a.
- Luces activadas por reglas, checkpoint, guardado rÃ¡pido, carga y victoria.

## Controles

| AcciÃ³n | Control |
|---|---|
| Mover | WASD |
| Mirar | RatÃ³n |
| Disparar | BotÃ³n izquierdo |
| Interactuar | E |
| Saltar | Espacio |
| Pausa | Escape |
| Guardado rÃ¡pido / carga rÃ¡pida | F5 / F9 |
| Rendimiento | F3 |

## Arte de personajes

Los cuatro retratos de cuerpo completo de `art/characters` fueron generados
para este proyecto con la herramienta de generaciÃ³n de imÃ¡genes de OpenAI y
despuÃ©s integrados como billboards transparentes de una celda. La direcciÃ³n
visual solicitada fue pixel art de terror de finales de los noventa, silueta
completa, fondo transparente y un Ãºnico personaje sin texto ni escenario.

Cada archivo `actors/*.actor` declara el tamaÃ±o exacto de su imagen como una
sola celda. Esto hace imposible que un recorte invÃ¡lido muestre el atlas entero.
Las animaciones actuales reutilizan esa pose; el editor puede reemplazarlas por
hojas con varias celdas sin cambiar las reglas ni las instancias del mapa.
