# 06 · Crear otro juego con el motor

## Empieza por el laboratorio

`retro_lab` es la demostración ejecutable de desacoplamiento. En CMake enlaza
`retro_platform`, que depende de `retro_core` y raylib. No enlaza `fps_game`, ni
usa sus texturas, marcadores de combate, salud, inventario o menús.

Para un segundo juego crea un directorio propio, por ejemplo `src/explorer`,
con un `main.c`, estado y funciones de actualización/dibujo. No copies `engine`.

```cmake
add_executable(explorer src/explorer/main.c)
retro_target(explorer)
target_link_libraries(explorer PRIVATE retro_platform)
```

Añade copia de tus recursos junto al ejecutable, usando el mismo patrón de
`retro_assets`. Los presets siguen construyendo todos los targets del proyecto.

## Contrato mínimo de una aplicación

1. Abrir plataforma y crear renderer.
2. Cargar un mapa y sus materiales.
3. Interpretar sus marcadores para crear tu estado.
4. Leer `ReInput`, acumularlo y extraer ticks con `ReClock`.
5. Actualizar cuerpos y reglas sin dibujar.
6. Construir una cámara y dibujar mundo, sprites y UI.
7. Presentar el framebuffer y liberar recursos al terminar.

Las APIs de render reciben vistas prestadas: puedes almacenar tus entidades
como te convenga y emitir `re_draw_billboard` por cada una. No necesitas adaptar
tu juego a un ECS ni a una clase base impuesta por el motor.

## Qué pertenece a cada lado

**Motor:** cálculo de un rayo, barrido de un cuerpo, rasterización de un triángulo,
carga geométrica y presentación de audio PCM.

**Juego:** qué significa un impacto, cuánto daño hace, cuándo se abre una puerta,
qué objetos hay, cómo se gana, qué texto muestra el HUD y qué sonido corresponde.

Por ejemplo, un juego de exploración puede usar un rayo para seleccionar objetos
sin tener armas. Un juego de terror puede interpretar marcadores `note` y
`trigger` sin añadir esos nombres al cargador del motor.

## Recursos propios

`re_texture_init` reserva píxeles; puedes generarlos, como el laboratorio, o
cargarlos con `re_platform_image_load`. Esta última produce una textura CPU,
no un objeto GPU. El renderer necesita leer sus texels.

Los sonidos se crean desde PCM mediante `re_platform_sound`, que copia los datos.
Guarda el ID retornado y reprodúcelo con volumen y pan. El ID `-1` representa
audio no disponible y reproducirlo no hace nada. No reutilices IDs de una
plataforma destruida.

Para materiales externos, comprueba que tu tabla cubre los índices usados por
el mapa. El formato v1 limita los IDs a 0–15; cada aplicación proporciona 16
slots. No liberes una textura mientras se está dibujando.

## Fronteras que conviene conservar

- No incluyas `raylib.h` en headers del motor o estado del juego.
- No añadas una función `engine_damage_guard`: eso pertenece a Foundry.
- No guardes punteros a entidades en el archivo del nivel.
- No hagas que render avance animaciones; usa tiempo de simulación.
- No añadas abstracciones para backends inexistentes antes de necesitarlas.

Un futuro renderer GPU puede ofrecer una interfaz de escena compatible, pero
no se implementa un sistema de plugins especulativo. La separación actual deja
localizados tanto los cálculos CPU como el adaptador de presentación.

## Ejercicio guiado: paseo sin combate

Usa `lab.map`, añade un marcador `goal` y un contador de objetos visitados en tu
estado. Dibuja una esfera representada por un billboard. Al acercarte, cambia
el contador y reproduce una nota. No modifiques ningún archivo de `src/engine`.
Si necesitas cambiar el núcleo para entender qué es `goal`, revisa la separación.
