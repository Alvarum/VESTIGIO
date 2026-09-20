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

Haunted es el showcase jugable de RetroForge. Su recorrido está diseñado para
demostrar sistemas reales del motor en una partida corta y legible, no para
ser una galería de objetos aislados.

## Recorrido

1. Hablar con Elena en el refugio y activar el tablero eléctrico.
2. Romper el vidrio del patio con dos disparos.
3. Derrotar al cuidador y recoger la llave que deja caer una sola vez.
4. Usar la llave en el panel del ala de servicio y subir la escalera.
5. Entrar al circuito superior, romper la línea de visión de la secuestradora
   y alcanzar la sala verde por cualquiera de sus dos rutas.
6. Activar el panel, cruzar la pasarela segura y combatir al Guardián.
7. Volver a la salida verde después de derrotarlo.

Los checkpoints están en el vestíbulo, antes de la persecución y al cerrar las
compuertas de seguridad. Una captura restaura el último estado completo.

## Sistemas demostrados

- Portales parciales, paredes conservadas alrededor de aberturas y colisión
  compartida entre render, movimiento, visión y disparos.
- Puerta abierta, ventana rompible, puerta con llave y compuertas controladas
  por reglas.
- Habitaciones con distinta altura y escalera construida con volúmenes 2.5D.
- NPC, conversación ramificada, variable global y objetivo visible.
- Pickup colocado, curación, drop al morir e inventario consumible.
- Perseguidora invulnerable, percepción, navegación y captura con retry.
- Jefe de dos fases: invocación limitada y proyectiles físicos bloqueados por
  la geometría.
- Luces activadas por reglas, checkpoint, guardado rápido, carga y victoria.

## Controles

| Acción | Control |
|---|---|
| Mover | WASD |
| Mirar | Ratón |
| Disparar | Botón izquierdo |
| Interactuar | E |
| Saltar | Espacio |
| Pausa | Escape |
| Guardado rápido / carga rápida | F5 / F9 |
| Rendimiento | F3 |

## Arte de personajes

Las cuatro hojas de `art/characters` fueron generadas para este proyecto con la
herramienta de imágenes de OpenAI. Cada una contiene cuatro poses coherentes,
fondo transparente y un pivote estable. La dirección visual fue terror
industrial de finales de los noventa, con siluetas completas y legibles.

Cada archivo `actors/*.actor` declara celdas de 192 × 256 píxeles y asigna poses
concretas a reposo, movimiento, ataque o captura. El renderer valida el recorte
y nunca usa el atlas entero como sustituto de una celda inválida.
