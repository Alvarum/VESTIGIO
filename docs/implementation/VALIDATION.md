# Verificación durante implementación

Este plan verifica **VESTIGIO**, dentro de `engines/vestigio/`. RetroForge está
en otro repositorio y sus pruebas no forman parte de esta matriz. Los comandos
de esta guía se ejecutan desde la raíz de VESTIGIO.

## Matriz baseline y cierre integrado

Desde la raíz del checkout preparado, en PowerShell:

```powershell
./tools/check.ps1 -Full
```

El chequeo verifica formato sin reescribir y ejecuta analyze, UBSan,
Debug y Release con pruebas. Analyze y UBSan desactivan las aplicaciones, así
que **no verifican GPU, ventana ni WPF**. Debug/Release compilan Studio .NET y
ejecutan CTest. Revisar exit code y salida real de cada etapa; proceso lanzado
no significa prueba terminada.

Para iterar en un cambio que necesite aplicaciones:

```powershell
./tools/build.ps1 -Preset debug -Test
```

Para cambios C independientes de ventana:

```powershell
./tools/build.ps1 -Preset analyze -Test
./tools/build.ps1 -Preset ubsan -Test
```

Una vez configurado y compilado el preset, se puede seleccionar la prueba existente pertinente:

```powershell
& './.tools/msys64/ucrt64/bin/ctest.exe' --test-dir build/vestigio/debug --output-on-failure -R '^vestigio_document$'
& './.tools/msys64/ucrt64/bin/ctest.exe' --test-dir build/vestigio/debug --output-on-failure -R '^vestigio_spatial$'
& './.tools/msys64/ucrt64/bin/ctest.exe' --test-dir build/vestigio/debug --output-on-failure -R '^vestigio_gpu_scene$'
```

No basta CTest si no se ha recompilado el candidato. Confirmar que el filtro encontró y ejecutó pruebas; cero tests no es PASS. Las pruebas nuevas deben registrarse con nombre y requisitos claros en CMake/CTest al incorporarse, sin inventar hoy comandos de targets futuros.

Selecciona pruebas `vestigio_gpu_*` según el cambio; una ventana
oculta también requiere contexto, driver y entorno gráfico válidos.

## Preparación y aislamiento

Comprobar `.tools/msys64/ucrt64/bin`, fuente raylib fijada en `.deps`, SDK .NET,
restore de VESTIGIO Studio y acceso al desktop. `tools/build.ps1` no restaura NuGet
por sí solo; `tools/bootstrap.ps1` prepara herramientas y restaura los proyectos
de Studio. No ejecutar bootstrap sobre recursos compartidos en
mitad de otras compilaciones. Registrar versiones realmente usadas.

Ejecutar builds sin writers concurrentes en ese checkout. Los directorios
`build/vestigio/<preset>/` mantienen separados objetos, binarios y pruebas de
cada perfil. Cada prueba que exporta/guarda usa una copia de proyecto dentro
de su carpeta de evidencia. La distribución VESTIGIO requiere su propio flujo
y aceptación antes de marcar sus tickets de entrega.

No instalar herramientas de captura ni cambiar configuración global sólo para obtener un PASS. Si falta contexto gráfico o dispositivo, registrar bloqueo de esas comprobaciones y ejecutar lo independiente. Un agente con acceso visual hace los recorridos necesarios; una aceptación humana pendiente no se inventa.

## Perfiles por ticket

Los códigos del backlog remiten a esta tabla. Ejecutar los casos nuevos relevantes al ticket; ampliar a matriz completa en baseline, al cerrar tramo integrado y ante cambios transversales de ABI/build. No repetir todo sin cambios ni escribir tests que sólo reflejen la implementación.

| Perfil | Evidencia requerida |
|---|---|
| V-CORE | Casos de contrato afectados, formato, analyze y UBSan; errores/capacidad/lifecycle cuando se tocan. No se exige compatibilidad con prototipos anteriores |
| V-APP | Debug build + pruebas de sesión/host afectadas y ejecución real; release al cerrar tramo. No sumar results de distintos commits |
| V-SDK | Instalar SDK en prefijo aislado; configurar/compilar/ejecutar consumidor fuera del repo, header C11 y C++, ningún include interno/.NET. API/stale handles/versiones según ticket |
| V-GPU | Geometría/shader en GPU real; captura de comandos o inspección equivalente del backend/programa/buffers. Depth/alpha/clipping, aspecto, resize, cleanup, contadores uploads/readbacks. Normal frame sin readback; screenshots puntuales etiquetados |
| V-WPF | Flujo en Studio real: foco, pressed/held/released, DPI 100/150/200 cuando disponibles, docking/minimizar/abrir-cerrar, Edit/Play y teclado. Fotos/capturas + resultados por caso; pruebas C# no certifican apariencia |
| V-ASSET | Fixture válida/inválida y aviso/licencia, ejes/pivots/normales/alpha, deduplicación, refcount, fallo transaccional, reimport y purge. Separar parser sin GPU de upload real |
| V-DATA | Round-trip semántico, UUID/refs, optional/required, versiones, locale/Unicode, save fallido/recuperación y expected revision. Leer de vuelta archivo real, no sólo memoria tras guardar |
| V-SPATIAL | Casos numéricos de ray/sweep/overlap, esquinas/pendientes/velocidad admitida y puerta obstruida/padre rotado. Recorrido visual con collider debug real |
| V-AUDIO | Ownership/voces/buses/streaming, evento correcto, pausa/cambio de mundo y error de dispositivo; escucha real registrada aparte de tests automatizados |
| V-PERF | Hardware/driver/commit/escena/preset, warmup/duración, p50/p95/p99 y CPU/GPU separados, contadores y recargas repetidas. VRAM estimada marcada como tal, limitaciones del equipo explícitas |
| V-DELIVERY | Dos consumidores externos, export sólo de dependencias, rutas con espacios/Unicode/cwd arbitrario, save/restore y CLI procesos reales. Revisión de avisos sin inventar licencia propia |

No todos los tickets implementan todo un perfil: sus criterios de aceptación fijan el subconjunto. E05/Z01 reúnen capacidades en un mismo recorrido. Las pruebas temporales de G01/G02 deben convertirse en corpus reutilizable; no borrar la única evidencia de la decisión de superficie.

## Gates de los tramos

| Tramo | Recorrido que debe comprobarse en candidato integrado |
|---|---|
| ARRANQUE | Crear/limpiar contexto y entidades desde C externo; draw de geometría GPU sintética; superficie Studio viable, input entre hosts y settings mínimos. No afirmar importación GLB todavía |
| BASE TÉCNICA | Consumidor C carga GLB, crea instancias compartidas y renderer GPU dibuja la escena de prueba; documento nuevo valida referencias y round-trip de componentes soportados |
| PRIMERA ESCENA (J01) | Proyecto nuevo arranca en Player; cámara se mueve sobre geometría visual con modelo importado, GPU real, resize y cierre limpio; no se exige colisión |
| MOVIMIENTO CON COLISIÓN (S01/S02) | Consultas espaciales y controlador dirigidos en Debug/Release/UBSan/Analyze, consumidor SDK externo, Player GPU real con barrera y overlay de colliders; revisión interactiva humana del recorrido por separado |
| AMPLIACIONES EDITORIALES | Para las funciones encargadas: crear/editar, undo, guardar, cerrar, reabrir y jugar; Stop conserva documento. Evidencia en Studio y Player cuando corresponda |
| DISTRIBUCIÓN (si se encarga) | Dos juegos con mismo SDK/runtime, uno principalmente C; export fuera del repo, save/restore, CLI con conflicto de revisión, matriz de regresión y medidas GPU reales |

## Registro de evidencia

Informes versionables en `docs/implementation/evidence/<ID>.md`; binarios/logs voluminosos en `artifacts/implementation/<ID>/<commit>/` o almacenamiento acordado. No crear artifacts ficticios ahora. En el informe indicar ruta/hash y si el fichero ignorado sólo existe localmente; no asumir que otro agente tiene esos archivos.

Cada informe contiene: commit/base, dirty state, entorno/toolchain/hardware, comando exacto, exit code, tests ejecutados/conteos, resultado por criterio, captura/log relevante, límites y reproducción. Clasificaciones `PASS`, `FAIL`, `NOT_RUN`, `BLOCKED_ENV`, `HUMAN_PENDING` por caso. Sólo PASS de todos los criterios necesarios permite VERIFIED; integración se comprueba después.

Un worker no puede rebajar criterios para que su ticket pase. Si cambia un criterio, registrar motivo/evidencia/contrato y revisar impacto antes de aceptarlo. Al cerrar, actualizar backlog y STATE con refs, no reescribir la investigación histórica como si el motor ya hubiera tenido esas capacidades.
