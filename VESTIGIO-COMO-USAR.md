# VESTIGIO: compilar, abrir y usar el Atrium 3D

Esta guía es para el motor **VESTIGIO 3D**. Sus fuentes, pruebas, Studio y
recursos están en `engines/vestigio/`. Ejecuta los comandos desde la **raíz del
repositorio**.

## Requisitos y primera preparación

- Windows con controlador gráfico capaz de crear un contexto **OpenGL 3.3**. El Atrium se renderiza en la GPU.
- SDK de **.NET 10** para Studio (interfaz WPF).
- **Python 3** para ejecutar la prueba integrada de importación en Player; no hace falta para abrir Studio o la demo.
- Conexión a Internet durante la primera preparación: el script descarga herramientas y dependencias dentro de `.tools`, `.deps` y `.nuget` del proyecto.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/bootstrap.ps1
```

La opción `ExecutionPolicy Bypass` sólo se aplica a ese proceso. No hace falta repetir `bootstrap.ps1` para cada ejecución si las herramientas ya están preparadas.

## Abrir la demo Player

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1
```

El script compila VESTIGIO y abre la escena `engines/vestigio/assets/demo/atrium.level.json`. El ejecutable queda en `build/vestigio/debug/bin/vestigio_player.exe`; al abrirlo con el script, éste copia los recursos demo junto al binario.

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

Para jugar otro nivel guardado desde Studio, pasa `-Level ruta\nivel.level.json`. Player carga los modelos GLB/glTF importados que figuran en el manifiesto de ese nivel y verifica sus huellas.

## Abrir el editor VESTIGIO Studio

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-studio-3d.ps1
```

El script compila el host GPU y la aplicación WPF, luego inicia **VESTIGIO Studio — Atrium 3D**. La aplicación queda en `build/vestigio/debug/bin/vestigio_studio.exe` y abre niveles 3D sin argumentos especiales. Para abrir un nivel propio directamente: `./build/vestigio/debug/bin/vestigio_studio.exe --level "ruta\nivel.level.json"`.

En **Editar**, haz clic sobre un objeto o selecciónalo en **Jerarquía**. Con **Añadir pilar** o **Duplicar** creas otra instancia; cambia posición, rotación o escala en el inspector y pulsa **Aplicar transformación**. **Deshacer/Rehacer** revierten o repiten cambios. **Guardar como…** crea una copia de trabajo sin sobrescribir el Atrium original; después usa **Guardar** y **Reabrir** para comprobar el archivo. La vista de edición permite cámara libre con WASD, botón derecho para mirar y Espacio/Ctrl para subir/bajar; también ofrece órbita, ortográfica y **Encuadrar selección**.

Para editar varios objetos a la vez, selecciónalos con **Ctrl** o **Mayús** en la jerarquía o en el viewport. Elige **Mover**, **Rotar** o **Escalar**, espacio **Mundo/Local**, pivote y **Paso**; arrastra el eje coloreado del gizmo GPU. El arrastre sigue el eje visto desde la cámara, incluso en órbita. **Escape** cancela sin ensuciar el documento; soltar confirma un solo paso de Deshacer. **Duplicar**, **Borrar** y **Cambiar padre** actúan sobre toda la selección; el motor rechaza ciclos de jerarquía y transformaciones que crearían shear. Guarda y reabre la copia para comprobar el resultado.

**+ Habitación con abertura** conserva la sala fija de la demo anterior alrededor del punto inicial, en un solo paso de Deshacer. Puedes añadir **una plantilla fija por nivel**; sus seis piezas protegidas no se duplican ni transforman individualmente. Para crear varias habitaciones editables, usa **Construcción · Habitaciones** más abajo. Guarda una copia y ábrela en Player con `-Level "ruta/nivel.level.json"` para jugarla fuera de Studio.

Pulsa **Probar** para jugar una instancia aislada del documento con colisiones, salto y puerta interactiva (**E**); **Detener** vuelve a Editar sin guardar los cambios ocurridos durante la prueba. El selector **Imagen** alterna Limpio/Retro. Los deslizantes de **Audio** ajustan volúmenes; el sonido se reproduce en Probar. Para usar otro archivo de preferencias, inicia Studio con `-Settings ruta.settings` (o el ejecutable directo con `--settings ruta.settings`).

Las dos piezas animadas aparecen sólo en **Probar**; **Editar** muestra el documento guardado sin ellas. Detener destruye esas instancias de juego.

## Importar modelos y organizar la escena (E03)

En **Editar**, abre **Biblioteca de assets** y pulsa **Importar GLB/glTF…**. Elige un `.glb` o un `.gltf` con buffers e imágenes embebidos. Al seleccionar el recurso aparece una vista previa temporal en el viewport GPU; no modifica el nivel. Si no puede cargarse, verás un placeholder y el motivo en **Problemas**. La biblioteca muestra el ID estable, el estado y la huella SHA-256 del archivo.

Pulsa **Colocar** para crear una entidad. Puedes colocar el mismo modelo varias veces: las entidades tienen UUID distintos y comparten un recurso GPU. Selecciónalas en **Jerarquía** con Ctrl/Mayús. El **Inspector** muestra campos tipados, unidades, rangos y referencias; cambia sólo los campos que quieras unificar y pulsa **Aplicar campos**. Los valores no tocados se conservan; el lote se deshace en un solo paso. Un valor inválido se marca junto a su campo y en **Problemas**.

En **Organización del editor** asigna grupo y capa. **Ocultar capa en editor** afecta la vista Editar; en **Probar** y Player esos objetos vuelven a verse. **Renombrar** cambia el nombre del recurso sin cambiar su ID; **Reimportar** actualiza el archivo conservando ese ID. Usa **Guardar como…** en una carpeta de trabajo: Studio copia los modelos importados a `assets/<ID>.glb` o `.gltf` junto al nivel. Después **Reabrir** comprueba el resultado, y puedes jugar el archivo con `tools/run-3d-demo.ps1 -Level "ruta\nivel.level.json"`.

Los `.gltf` que dependen de archivos externos se rechazan con un diagnóstico; para este flujo expórtalos como `.glb` o con datos embebidos. Si modificas un modelo copiado sin reimportarlo, Studio/Player detectan que su huella ya no coincide.

## Construir habitaciones y aberturas (E04)

En **Editar**, abre **Construcción · Habitaciones**. Para empezar rápido, introduce posición y dimensiones en metros y pulsa **Trazar**: el panel prepara cuatro vértices. También puedes escribir un contorno `x,y`, un vértice por línea, en sentido antihorario. **Planta Z**, alto y grosor controlan la receta; las cotas y la cuadrícula del plano ayudan a revisarla.

Para abrir un muro, elige una arista (la primera es `0`), el tipo **puerta**, **ventana** o **hueco**, su distancia desde el inicio de la arista, ancho, alto y antepecho. Pulsa **Añadir abertura** y después **Vista previa**. La vista previa no guarda objetos: **Cancelar** la retira sin crear historial. **Crear habitación** confirma la receta en una sola acción de Deshacer. Selecciona la habitación en **Jerarquía** para cambiar su contorno o aberturas y pulsa **Actualizar habitación**; Deshacer/Rehacer regenera la malla y la colisión juntas.

Puedes crear otra habitación con distinta **Planta Z**. En el plano de autoría, **Cuadrícula** muestra la referencia métrica y **Otras plantas translúcidas** muestra sus contornos como guía. Esos controles son editoriales: **Probar** y Player muestran todas las plantas. Guarda una copia con **Guardar como…**, usa **Reabrir** y después **Probar** para recorrerla; el vano abierto se atraviesa y la pared contigua bloquea.

Las aberturas de esta receta son vacíos en el muro: una abertura de tipo puerta no crea todavía una hoja móvil, y la de ventana no añade marco ni vidrio. Esos objetos y la edición avanzada entre pisos tienen tickets posteriores.

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
- **No aparecen recursos del Atrium al lanzar el `.exe` manualmente:** usa los scripts desde la raíz para recompilar y copiar los recursos; verifica que `atrium.gltf`, `atrium.level.json` y `audio/*.wav` estén en `build/vestigio/debug/bin/assets/demo/`. Los originales viven en `engines/vestigio/assets/demo/`.
- **No se aprecian luces o niebla:** recompila con los scripts para copiar el glTF y nivel corregidos, prueba el Atrium original y revisa `visual=... lights=... fog=...` en el Player. Las luces y la niebla provienen del `.level.json`; F6 sólo cambia Limpio/Retro. La [comparación de referencia](docs/implementation/evidence/W06-fix.md) muestra el efecto esperado desde la misma cámara.
- **No hay sonido:** confirma que Windows tenga una salida de audio activa, revisa los deslizantes y F7/F8, y verifica los dos WAV en `engines/vestigio/assets/demo/audio`. Puedes aislar un problema de dispositivo ejecutando la demo con `-NoAudio`; el juego debe seguir funcionando sin sonido.

Para las opciones de la demo, revisa `tools/run-3d-demo.ps1` o ejecuta el binario con una opción inválida para ver su sintaxis. Entre las opciones útiles están `-ShowColliders`, `-Settings`, `-Visual`, `-VolumeMaster`, `-VolumeSfx`, `-VolumeAmbience`, `-SaveAudio`, `-NoAudio`, `-Smoke` y `-Capture`.
