# VESTIGIO: compilar, abrir y usar el Atrium 3D

Esta guía es para el **motor VESTIGIO nuevo** y su escena de ejemplo Atrium 3D. Los ejecutables `retro_fps`, `retro_lab` y el Studio abierto sin `--atrium` corresponden a otras partes del repositorio. Ejecuta los comandos siguientes desde la **raíz de este repositorio** en PowerShell.

## Requisitos y primera preparación

- Windows con controlador gráfico capaz de crear un contexto **OpenGL 3.3**. El Atrium se renderiza en la GPU.
- SDK de **.NET 10** para Studio (interfaz WPF).
- Conexión a Internet durante la primera preparación: el script descarga herramientas y dependencias dentro de `.tools`, `.deps` y `.nuget` del proyecto.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

La opción `ExecutionPolicy Bypass` sólo se aplica a ese proceso. No hace falta repetir `bootstrap.ps1` para cada ejecución si las herramientas ya están preparadas.

## Abrir la demo Player

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1
```

El script configura y compila `vestigio_player`, y luego abre la escena `assets/demo/atrium.level.json`. El ejecutable queda en `build/vestigio-demo/bin/vestigio_player.exe`; sus recursos se copian junto al binario durante la compilación. Si quieres iniciarlo directamente después de compilar, usa `./build/vestigio-demo/bin/vestigio_player.exe`.

| Control | Acción en Player |
|---|---|
| WASD | Moverse con colisiones |
| Ratón | Mirar alrededor |
| Espacio | Saltar |
| E | Abrir o cerrar la puerta al acercarte y apuntarla |
| F3 | Mostrar u ocultar colisionadores |
| F6 | Alternar imagen Limpio/Retro |
| F7 / F8 | Bajar / subir volumen general |
| Escape | Cerrar la demo |

El Player imprime el nombre de la GPU y un resumen `visual=... lights=... fog=...` en la consola. Para elegir el perfil al arrancar: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1 -Visual clean` (o `retro`). Los ajustes personales se guardan por defecto en `%LOCALAPPDATA%\VESTIGIO\visual.settings`; `-Settings ruta.settings` permite usar otro archivo. El nivel JSON contiene las luces y la niebla de la escena.

Delante del punto inicial hay dos piezas cian que flotan y giran con fases distintas. Son actores temporales de la demo: no bloquean el paso ni se guardan en el nivel.

Para una captura reproducible que se cierra sola:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1 -Smoke 8 -Capture build/atrium.png
```

Para comparar otro nivel guardado desde Studio, pasa `-Level ruta\nivel.level.json`. El Player usa el modelo `atrium.gltf` de esta demo; esta opción no importa por sí sola modelos arbitrarios.

## Abrir el editor VESTIGIO Studio

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-studio-3d.ps1
```

El script compila el host GPU y la aplicación WPF, luego inicia **VESTIGIO Studio — Atrium 3D**. La aplicación queda en `build/vestigio-studio/bin/retro_studio.exe`. Si se abre directamente, usa `./build/vestigio-studio/bin/retro_studio.exe --atrium`: sin `--atrium` abre otro proyecto. Para abrir un nivel propio directamente: `./build/vestigio-studio/bin/retro_studio.exe --atrium --level "ruta\nivel.level.json"`.

En **Editar**, haz clic sobre un objeto o selecciónalo en **Jerarquía**. Con **Añadir pilar** o **Duplicar** creas otra instancia; cambia posición, rotación o escala en el inspector y pulsa **Aplicar transformación**. **Deshacer/Rehacer** revierten o repiten cambios. **Guardar como…** crea una copia de trabajo sin sobrescribir el Atrium original; después usa **Guardar** y **Reabrir** para comprobar el archivo. La vista de edición permite cámara libre con WASD, botón derecho para mirar y Espacio/Ctrl para subir/bajar; también ofrece órbita, ortográfica y **Encuadrar selección**.

**+ Habitación con abertura** añade una sala fija alrededor del punto inicial, sobre el piso del Atrium, en un solo paso de Deshacer. El vano frontal se puede cruzar en Probar; la pared contigua bloquea. Puedes añadir **una por nivel**. Sus seis piezas protegidas no se duplican ni transforman individualmente; los pilares normales sí. Guarda una copia y ábrela en Player con `-Level "ruta/nivel.level.json"` para jugarla fuera de Studio.

Pulsa **Probar** para jugar una instancia aislada del documento con colisiones, salto y puerta interactiva (**E**); **Detener** vuelve a Editar sin guardar los cambios ocurridos durante la prueba. El selector **Imagen** alterna Limpio/Retro. Los deslizantes de **Audio** ajustan volúmenes; el sonido se reproduce en Probar. Para usar otro archivo de preferencias, inicia Studio con `-Settings ruta.settings` (o el ejecutable directo con `--settings ruta.settings`).

Las dos piezas animadas aparecen sólo en **Probar**; **Editar** muestra el documento guardado sin ellas. Detener destruye esas instancias de juego.

## Compilar y ejecutar pruebas

Los scripts de apertura ya compilan sólo lo necesario. Para compilar todos los targets y correr las pruebas del preset:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Preset debug -Test
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build.ps1 -Preset release -Test
```

`tools/check.ps1` ejecuta formato y análisis; `-Full` añade Debug y Release. Las pruebas automatizadas no sustituyen inspeccionar la imagen ni escuchar el ambiente. `tools/run-studio-3d.ps1 -NoLaunch` sólo compila Studio. La documentación y los resultados por oleada están en `docs/implementation/evidence/`.

## Si algo falla

- **Falta `cmake.exe` o falla la restauración de .NET:** ejecuta `tools/bootstrap.ps1`; confirma `dotnet --version` y que el SDK instalado sea 10.
- **No se abre la ventana o falla el shader:** actualiza el controlador de la GPU y comprueba que el dispositivo exponga OpenGL 3.3. El Player imprime la GPU detectada; un contexto OpenGL por software no verifica la ruta GPU real.
- **No aparecen recursos del Atrium al lanzar el `.exe` manualmente:** usa los scripts desde la raíz para recompilar y copiar `assets/demo`; verifica que `atrium.gltf`, `atrium.level.json` y `audio/*.wav` estén junto al binario en `assets/demo/`.
- **No se aprecian luces o niebla:** recompila con los scripts para copiar el glTF y nivel corregidos, prueba el Atrium original y revisa `visual=... lights=... fog=...` en el Player. Las luces y la niebla provienen del `.level.json`; F6 sólo cambia Limpio/Retro. La [comparación de referencia](docs/implementation/evidence/W06-fix.md) muestra el efecto esperado desde la misma cámara.
- **No hay sonido:** confirma que Windows tenga una salida de audio activa, revisa los deslizantes y F7/F8, y verifica los dos WAV en `assets/demo/audio`. Puedes aislar un problema de dispositivo ejecutando la demo con `-NoAudio`; el juego debe seguir funcionando sin sonido.

Para las opciones de la demo, revisa `tools/run-3d-demo.ps1` o ejecuta el binario con una opción inválida para ver su sintaxis. Entre las opciones útiles están `-ShowColliders`, `-Settings`, `-Visual`, `-VolumeMaster`, `-VolumeSfx`, `-VolumeAmbience`, `-SaveAudio`, `-NoAudio`, `-Smoke` y `-Capture`.
