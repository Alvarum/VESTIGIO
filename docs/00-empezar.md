# 00 · Empezar, jugar y estudiar

## Qué vas a construir mentalmente

Un mapa es un conjunto de polígonos y alturas. El jugador vive en ese mundo.
Una cámara transforma sus puntos a coordenadas relativas al observador. El
renderer decide qué píxel corresponde a cada superficie. raylib muestra ese
array de píxeles, recibe teclado/ratón y reproduce audio.

Puedes estudiar todo este recorrido sin aprender un editor externo. Empieza
por el laboratorio: allí no hay enemigos ni armas que distraigan del motor.

## Preparar Windows

Abre PowerShell en la raíz. El bootstrap descarga MSYS2 en `.tools/msys64` y
raylib y raygui en `.deps`; no modifica `C:/msys64` ni el PATH permanente.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Preset debug -Test
./build/debug/bin/retro_lab.exe
./build/debug/bin/retro_fps.exe
./build/debug/bin/retro_studio.exe
```

`-ExecutionPolicy Bypass` afecta sólo a ese proceso. No es necesario cambiar
la política global de Windows. Si tu terminal ya permite scripts, puedes usar
directamente `./tools/build.ps1 -Preset debug -Test`.

La primera descarga necesita varios GB disponibles para bibliotecas, compiladores
y cachés. Las siguientes compilaciones reutilizan la instalación y el archivo
fijado de raylib. No borres `.tools` mientras un compilador o depurador lo use.

### Qué hace cada herramienta

| Herramienta | Trabajo |
|---|---|
| GCC | Convierte C en código objeto y participa en el enlace. |
| CMake | Describe dependencias y genera las reglas de compilación. |
| Ninja | Ejecuta sólo las reglas necesarias cuando cambia un archivo. |
| CTest | Ejecuta los programas de prueba y comprueba su código de salida. |
| GDB | Detiene la ejecución, inspecciona variables y recorre llamadas. |
| clang-format | Mantiene un estilo uniforme; no demuestra corrección. |
| Analizador de GCC | Busca errores siguiendo posibles caminos del programa. |

## Tu primera sesión

1. Abre `retro_lab`. Recorre las dos habitaciones y cruza el escalón.
2. Activa F2 y observa la triangulación. Activa F3 y mira cómo cambia la profundidad.
3. Mira arriba, abajo y salta cerca de una pared. El recorte mantiene los triángulos válidos.
4. Abre `retro_fps`. Las flechas y Enter navegan por los menús.
5. Avanza al depósito naranja de la derecha, recoge la llave y vuelve a la nave.
6. Frente a la compuerta, pulsa E. Cruza y acércate a la terminal de salida.

Los guardias te persiguen al verte y atacan de cerca. Dos disparos eliminan a un
guardia. Disparar hacia una pared no daña a quien esté detrás. El botiquín se
conserva si ya tienes vida completa. Escape pausa; perder foco también pausa.

## Orden de lectura

1. `include/retro/math.h`: tipos pequeños, funciones puras y unidades.
2. `src/lab/main.c`: ciclo completo sin reglas de combate.
3. `include/retro/render.h` y `src/engine/render.c`: memoria y píxeles.
4. `src/engine/input.c`: eventos y tiempo fijo.
5. `src/engine/map.c` y `src/engine/world.c`: datos y geometría física.
6. `src/fps/game.c`: comportamiento expresado como estados.
7. `src/fps/main.c`: composición del juego, recursos y plataforma.
8. `tests/test_main.c`: experimentos reproducibles de los contratos anteriores.

Lee cada bloque junto al capítulo correspondiente. No hace falta entender todo
el renderer antes de ejecutar el primer experimento.

## Depurar

```powershell
$env:PATH = "$PWD/.tools/msys64/ucrt64/bin;$env:PATH"
./.tools/msys64/ucrt64/bin/gdb.exe ./build/debug/bin/retro_fps.exe
```

Dentro de GDB:

```text
break fps_game_tick
run
print g->player.position
next
step
backtrace
continue
```

`next` ejecuta una llamada completa; `step` entra en ella. `backtrace` muestra
quién llamó a la función actual. Compila en Debug para estudiar variables sin
las transformaciones del optimizador. Una ventana detenida por un breakpoint
puede parecer bloqueada: su hilo tampoco está procesando eventos.

## Errores habituales

- **No existe CMake:** ejecuta bootstrap; la instalación global no es necesaria.
- **C23 no disponible:** usa el GCC local. El proyecto rechaza compiladores cuyo
  `__STDC_VERSION__` no alcance `202311L`; no acepta silenciosamente C11/C17.
- **SHA incorrecto:** detén la preparación y verifica el archivo/origen. No quites
  la comprobación para hacerlo funcionar.
- **No se abre OpenGL:** revisa el controlador gráfico. El rasterizado del mundo
  es CPU, pero la presentación elegida usa OpenGL 3.3 de raylib.
- **Sin audio:** el juego imprime un aviso y sigue siendo jugable.
- **Mapa no encontrado:** conserva `assets` junto al ejecutable; la resolución
  de rutas no depende de la carpeta desde la que lo lanzaste.
- **Cambiaste un mapa fuente:** vuelve a ejecutar build para copiarlo a `bin/assets`.
  También puedes ejecutar el FPS con `--map ruta/al/archivo.map`.

## Entrega portátil

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Preset release -Test -Package
```

`dist/RetroForge` contiene los cuatro ejecutables, proyectos, mapas, documentación,
avisos y licencias. El ZIP puede
extraerse en otra carpeta. Conserva la estructura interna. La biblioteca de
ejecución de GCC se enlaza estáticamente; Windows aporta sus DLL del sistema.
