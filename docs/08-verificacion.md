# 08 · Verificación y límites

## Entorno

Windows 11, build 26200, CPU AMD Ryzen 5 3600. GCC 16.2.0, Clang 22.1.8,
CMake 4.4.3, Ninja 1.13.2 y raylib 6.0. Detalle y hashes en
`toolchain-versions.txt`. Fecha de esta entrega: 2026-09-20.

No se modificó el MSYS2 global. La investigación original conserva su SHA256.
Los ejemplos se compilan como C23; los warnings del código propio son errores.

## Comprobaciones automatizadas

`retro_contracts` ejecuta quince grupos sin ventana:

| Grupo | Evidencia cubierta |
|---|---|
| `geometry` | Vectores, normalización de cero, proyección, rayos paralelos, barrido y extremos. |
| `input_clock` | Pulsaciones/ratón sin duplicar, teclas mantenidas, recuperación, pausa e interpolación angular. |
| `maps` | Mapa real, IDs, degeneración, marcadores inválidos, versión, NaN, NUL y conservación del destino al fallar. |
| `collisions` | Sliding, esquina, movimiento rápido, escalón, techo bajo, puerta cerrada/abierta, salto y BFS. |
| `rendering` | Recursos, profundidad, transparencia, cruce del near plane, valores válidos y exportación. |
| `renderer_contracts` | Oráculo UV que detecta interpolación afín, diagonal sin grietas y sprite parcialmente oculto. |
| `utf8_text` | Decodificación UTF-8, glifos españoles y una única celda por carácter visible. |
| `game_journey` | Menú → combate → llave → puerta → salida, reinicio, pausa, pérdida de foco lógica, derrota y nuevo intento. |
| `shooting_blocked` | La puerta bloquea disparos; al abrirla el mismo tiro daña al guardia. |
| `solid_actors` | Un guardia vivo bloquea al jugador; su cadáver permite pasar. |
| `studio_world` | Proyecto v2, plantas coincidentes en XY, búsqueda 3D, canales de puerta/vidrio y serialización. |
| `reusable_gameplay` | Animación direccional, percepción/seguimiento constante, A*, escalera, captura única y jefe de dos fases. |
| `safe_door` | Una puerta que encuentra un cuerpo al cerrarse se detiene y vuelve a abrir. |
| `dynamic_lighting` | Luz puntual, cuantización retro y conservación de profundidad. |
| `interaction_journey` | Reglas, diálogo, energía, drop único, inventario, puerta, checkpoint exacto, save, CRC y roundtrip. |
| `rule_cycle_guard` | Orden FIFO y corte determinista de una cadena cíclica al agotar presupuesto. |

El piloto de `game_journey` usa acciones de movimiento, apuntado, disparo e
interacción. No concede la llave, no aumenta vida y no teletransporta al jugador.
Las pruebas de situaciones puntuales sí construyen fixtures explícitos para
aislar un caso, como un enemigo detrás de una compuerta.

`retro_window` abre una ventana oculta real de raylib/OpenGL, redimensiona a
1000×700 y 1440×810, presenta patrones, lee el framebuffer GPU y verifica colores
en el viewport y las bandas. También exporta y carga PNG. No es un mock.

Los grupos pasan en Debug y Release. El núcleo y el juego pasan además el
análisis estático de GCC y las pruebas instrumentadas con UBSan/Clang en modo trap.
No se ejecutó AddressSanitizer; UBSan no sustituye esa cobertura.

## Repetir la verificación

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/check.ps1 -Full
```

El script comprueba formato sin reescribir archivos, ejecuta análisis GCC,
UBSan/Clang y después las configuraciones Debug y Release con CTest.

Para pruebas sin ventana:

```powershell
./.tools/msys64/ucrt64/bin/ctest.exe --test-dir build/debug -LE window --output-on-failure
```

Para inspeccionar el resumen de los quince grupos:

```powershell
./build/debug/retro_tests.exe
```

## Revisión visual

Se generaron y revisaron imágenes reales del framebuffer de las aplicaciones:

- FPS: escena inicial, menú, cámara elevada, cercanía a pared, profundidad y geometría con automapa.
- Laboratorio: escena normal, cámara elevada y geometría.
- Studio: planta editable, inspector de sectores/personajes/animaciones y modo
  de prueba usando la misma simulación del reproductor.
- Juego exportado: escalera, perseguidora animada y HUD desde una carpeta con espacios.
- Se comprobó legibilidad de HUD/menú, cobertura de superficies, perspectiva,
  transparencia y ausencia de desbordamiento en las vistas observadas.

Las capturas locales se guardan en `artifacts`, ignorada por control de versiones.
Son salidas de la aplicación, no mockups. La prueba separada de ventana verifica
que la presentación GPU corresponde a los píxeles y al escalado esperado.

```powershell
New-Item -ItemType Directory -Force artifacts
./build/release/bin/retro_fps.exe --smoke 240 --capture artifacts/fps.png
./build/release/bin/retro_fps.exe --smoke 60 --capture artifacts/menu.png --menu --no-audio
./build/release/bin/retro_fps.exe --smoke 60 --capture artifacts/pitch.png --view 1 --no-audio
./build/release/bin/retro_fps.exe --smoke 60 --capture artifacts/near.png --view 2 --no-audio
./build/release/bin/retro_fps.exe --smoke 60 --capture artifacts/depth.png --view 3 --no-audio
./build/release/bin/retro_fps.exe --smoke 60 --capture artifacts/debug.png --view 4 --no-audio
./build/release/bin/retro_lab.exe --smoke 120 --capture artifacts/lab.png
./build/release/bin/retro_player.exe --project ./build/release/bin/assets/studio/haunted.retro `
  --smoke 120 --capture artifacts/player.png
```

`--smoke` congela la simulación y dibuja un número acotado de frames en ventana
oculta. No representa una partida completa ni prueba el tacto del ratón.
En el laboratorio `--view 1` es profundidad, `2` geometría, `3` pitch y `4` pared.

## Rendimiento observado

Mediciones Release, resolución interna 480×270, sobre este equipo:

| Escena fija | Frames | CPU render medio | Máximo observado |
|---|---:|---:|---:|
| FPS inicial | 240 | 3.569 ms | 5.858 ms |
| FPS elevado | 60 | 3.736 ms | 5.419 ms |
| FPS junto a pared | 60 | 3.263 ms | 4.403 ms |
| FPS profundidad | 60 | 4.887 ms | 6.448 ms |
| FPS geometría/automapa | 60 | 3.605 ms | 4.767 ms |
| Laboratorio normal | 180 | 3.189 ms | No registrado en este ejecutable |
| Proyecto Haunted exportado | 120 | 3.364 ms | 4.602 ms |
| Haunted con 4 luces (esta actualización) | 240 | 6.999 ms; p95 8.404 ms | 9.700 ms |
| Haunted optimizado Release | 600 | 6.942 ms; p95 9.223 ms | 11.069 ms |
| Haunted desde Studio/Debug | 600 | 7.485 ms; p95 9.696 ms | 11.565 ms |

El presupuesto de 60 FPS es 16.67 ms por frame. Estas medidas cubren el renderer
y la UI CPU; no deben invertirse y presentarse como FPS garantizados del juego.
La salida actual del FPS incluye también `frame_mean_ms`, que mide el bucle
con presentación en el smoke. Las ventanas ocultas y escenas fijas no representan
todos los escenarios de carga. No hay garantía para hardware o niveles distintos.

La medición nueva informa además 10 marcadores/entidades de escena, 4 luces y
9.89 MiB de memoria CPU contabilizada por el reproductor (estado, framebuffer,
profundidad y texturas CPU). No incluye memoria interna del driver gráfico.

El núcleo CPU se compila con `-O2` también en Debug; gameplay, editor y la
aplicación conservan `-Og`. Antes de esa separación, el mismo Haunted tardaba
29.735 ms de render medio en Debug. La optimización reduce ese valor a 7.485 ms
sin cambiar la resolución ni desactivar luces.

La última ejecución del paquete extraído (240 frames) registró 3.619 ms de render
CPU medio, 5.777 ms máximo y **4.193 ms por frame incluyendo presentación**.
El laboratorio del mismo paquete registró 3.435 ms de render medio en 120 frames.

El dispositivo de audio se inicializó correctamente (`audio=ready`). La fidelidad
audible de los efectos no se validó mediante escucha humana.

## Paquete y compatibilidad

La inspección de imports de los ejecutables muestra DLL del sistema Windows,
sin dependencia de las DLL del compilador ni de MSYS2. El código propio y raylib
se enlazan estáticamente. El paquete incluye mapas y avisos de dependencias.

Se reconstruyeron Debug y Release con `--clean-first` y ambas pasaron CTest.
Después se creó `dist/RetroForge-Windows.zip`, se extrajo en
`artifacts/Portable test with spaces` y se ejecutaron ambos programas desde
`artifacts/Empty working directory`, con PATH reducido a directorios de Windows.
Ambos terminaron con código 0 y exportaron las capturas solicitadas. Esta prueba
comprueba la independencia del directorio de trabajo y del PATH del compilador;
no equivale a una prueba en otra máquina física.

Además, `tools/export-project.ps1` generó `haunted.exe`, su proyecto y licencias
en `artifacts/Final export with spaces`, junto con `haunted-Windows.zip`. El
ejecutable se inició desde `artifacts/Empty working directory`, encontró
`project.retro` junto a sí mismo, dibujó 120 frames y guardó una captura con
código de salida 0.

Windows es la plataforma validada. El núcleo no incluye APIs de Windows, pero
esto no equivale a haber probado el juego en Linux/macOS.

## Comprobación manual restante

Las transiciones de pausa y foco están cubiertas en el estado del juego; la
presentación y el resize se probaron en una ventana real. Queda la valoración
humana de sensibilidad del ratón, audio audible, comodidad de combate, cambio
de foco mediante Alt+Tab y cierre usando el control de la ventana del escritorio.
También queda una sesión manual larga que cree un proyecto completo sólo con
clics desde cero; la carga, edición, guardado, recarga, comportamiento y
exportación sí tienen pruebas automatizadas o ejecuciones reproducibles. No se
declara esa aceptación humana como realizada por las pruebas automatizadas.
