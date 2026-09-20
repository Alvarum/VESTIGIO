# Tickets de implementación

Generado desde [backlog.json](backlog.json). Editar el JSON y ejecutar `python docs/implementation/validate-plan.py --render`; no mantener dos versiones manuales.

Los paths son puntos de entrada; directorios nuevos son propuestas hasta F01. Antes de escribir se reclama una lista exacta de archivos. Los perfiles se definen en [VALIDATION.md](VALIDATION.md).

## F00 — Reconciliar baseline y obtener evidencia inicial

Fase: **P0** · Rol: **integrator** · Estado: **INTEGRATED**.

Dependencias integradas: ninguna; inicio del plan.

Locks: `integration`.

Puntos de entrada: `docs/implementation/STATE.md`, `docs/implementation/evidence/`, `tools/build.ps1`, `tools/check.ps1`, `CMakePresets.json`.

**Trabajo:**

1. Registrar HEAD, git status y diff del código desde ae48d31; mapear A01-A14 del informe a vigente, corregido o pendiente de reproducción. No reaplicar correcciones ya existentes.
2. Comprobar toolchain/dependencias y ejecutar la matriz baseline de VALIDATION.md en un checkout aislado o sin escritores concurrentes; registrar fallos previos sin expandir el alcance de reparación.
3. Seleccionar corpus Haunted, portales parciales, sesión/editor y escenas sintéticas futuras; fijar ruta de evidencia, hardware y propietario de integración.

**Aceptación:**

- Baseline exacto y comandos/resultados registrados; ninguna afirmación PASS sin ejecución.
- Los fallos que impiden validar la futura vertical se resuelven con un ticket acotado o se identifican como bloqueo; fallos ajenos se preservan y documentan.
- Todos los agentes pueden reproducir el checkout y las herramientas; no comparten build/bin/obj ni editan una rama de integración simultáneamente.

Verificación: V-CORE, V-APP.

Desbloquea: Contratos e implementación sobre una base conocida.

Evidencia: docs/implementation/evidence/F00.md.

Commit integrado: `fe43b83de2f8579ecad715991de5c04252d61dd0`.

## F01 — Congelar contratos mínimos entre agentes

Fase: **P0** · Rol: **integrator** · Estado: **VERIFIED**.

Dependencias integradas: F00.

Locks: `public-api`, `build`, `contracts`.

Puntos de entrada: `docs/implementation/CONTRACTS.md`, `include/vestigio/`, `src/runtime/`, `src/render/`, `CMakeLists.txt`, `tests/sdk/`.

**Trabajo:**

1. Convertir C01-C08 de CONTRACTS.md en declaraciones mínimas de tipos, responsabilidades y errores; fijar nombres y export macros sin implementar toda la API hipotética del informe.
2. Preparar límites de targets Runtime/Tooling/backend y lugar de pruebas; preservar targets actuales. Definir qué crea/posee ventana y contexto y cómo se integra código nuevo.
3. Versionar contrato v0.1 y acordar con los responsables de GPU/runtime los puntos que el spike puede ajustar. Añadir consumidores mínimos de header C11/C++ sin arrastrar tipos internos.

**Aceptación:**

- Header público mínimo compilable en C11 y C++; sin raylib, ReProject, HWND ni dependencias WPF en la API de juego.
- Ownership, IDs, convención espacial, superficie y documento tienen un único contrato publicado; los nombres de directorios nuevos se registran.
- G01 y R01 pueden avanzar en archivos disjuntos; la incertidumbre de embedding está explícita y no se declara resuelta.

Verificación: V-CORE, V-SDK.

Desbloquea: Trabajo paralelo GPU/runtime con integración definida.

Evidencia: docs/implementation/evidence/F01.md.

## G01 — Primer frame de geometría GPU en ventana propia

Fase: **P1** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias integradas: F01.

Locks: `gpu-backend`, `platform-window`.

Puntos de entrada: `src/render/gpu_raylib/`, `src/platform/raylib_platform.c`, `src/lab/`, `tests/gpu/`.

**Trabajo:**

1. Crear backend raylib/rlgl con un dueño de contexto y recursos mesh, textura, shader y render target; escena sintética independiente del juego incorporado.
2. Dibujar malla y billboard en color/depth GPU; upscale nearest/letterbox configurable sin readback normal; conservar renderer CPU existente.
3. Añadir contadores uploads/readbacks/draws/recursos y captura solicitada; probar fallos de shader/target y cleanup parcial.

**Aceptación:**

- Captura o inspección de comandos acredita draw de geometría en GPU; UpdateTexture del framebuffer CPU no satisface el ticket.
- Resize, depth, alpha-cutout y aspecto funcionan con 320x180 y 640x360; una captura puntual es distinta del camino normal sin readbacks.
- Abrir/cerrar y errores liberan recursos propios; escenario y hardware quedan registrados.

Verificación: V-CORE, V-APP, V-GPU.

Desbloquea: Backend real para modelos y prueba WPF.

## R01 — Contexto, mundo, entidades y Transform con handles

Fase: **P2** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias integradas: F01.

Locks: `runtime-core`, `public-api`.

Puntos de entrada: `include/vestigio/`, `src/runtime/`, `src/world/`, `src/engine/math.c`, `tests/runtime/`.

**Trabajo:**

1. Implementar contexto/mundos y pools por tipo con handles generacionales validados por contexto/mundo; UUID documental separado.
2. Implementar TRS local/world y padre opcional, rechazo de ciclos y política explícita de shear/escala; operaciones fallidas transaccionales.
3. Añadir creación/destrucción diferida durante iteración y reserva de capacidad; mantener ReWorld/ReEntityId legacy detrás de adaptadores sin renombrado global.

**Aceptación:**

- Stale handle, tipo/contexto/mundo incorrecto y agotamiento de capacidad devuelven error sin UB ni truncado.
- Reparent preserve-world/local cumple contrato; ciclos y transform no finito se rechazan dejando estado anterior.
- Crear/destruir mundos repetidamente y fallar allocations no deja recursos retenidos; pruebas no necesitan GPU.

Verificación: V-CORE, V-SDK.

Desbloquea: Recursos, game callbacks y documentos genéricos.

## G02 — Resolver superficie GPU de Studio mediante spike

Fase: **P1** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias integradas: G01.

Locks: `gpu-backend`, `platform-window`, `wpf-viewport`.

Puntos de entrada: `src/studio/Controls/GameViewport.cs`, `src/studio/Controls/`, `src/studio/Native/`, `src/platform/`, `tests/studio/`, `docs/implementation/CONTRACTS.md`.

**Trabajo:**

1. Probar HwndHost/superficie nativa con el backend G01 y fijar lifecycle/foco/DPI/contexto; no asumir que GetWindowHandle permite adoptar cualquier HWND.
2. Mantener una superficie/contexto inicial y documentar relación con Edit/Play, docking y ventana Player; dibujar overlays de viewport en GPU.
3. Si raylib impide hosting correcto, comparar adaptación acotada de plataforma frente a otra superficie y documentar decisión. No reescribir Studio ni introducir D3D/Vulkan completos por defecto.

**Aceptación:**

- 50 ciclos abrir/cerrar, resize/minimizar, foco/Tab/Escape, DPI y docking cuentan con evidencia según entorno disponible; capacidades sin probar se declaran.
- El camino normal embebido presenta GPU sin copia por frame a WriteableBitmap; ventana externa sola no cierra este ticket.
- Decisión de superficie y límites está publicada; si no hay vía viable, ticket BLOCKED y continuidad de runtime/modelos independiente, nunca PASS ficticio.

Verificación: V-APP, V-GPU, V-WPF.

Desbloquea: Autoría 3D integrada; gate arquitectónico de WPF.

## R02 — Registro de assets y ownership CPU/GPU mínimo

Fase: **P2** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias integradas: R01.

Locks: `asset-core`, `public-api`.

Puntos de entrada: `src/assets/`, `include/vestigio/`, `tests/assets/`.

**Trabajo:**

1. Implementar AssetId, handles y estados loading/ready/failed para Texture/Mesh; catálogo fuente/meta separado de residente.
2. Definir acquire/release y retenciones de componentes; deduplicar por identidad/versión/opciones, no por cada instancia o alias de ruta.
3. Separar decode CPU de upload/release del hilo gráfico y definir reemplazo candidato; contabilidad RAM/VRAM estimada, purge y errores.

**Aceptación:**

- Dos usuarios comparten un recurso y destruir uno no invalida otro; refcounts no liberan objetos de GPU fuera de su contexto.
- Fallo de parse/allocation/upload no publica un asset medio válido y conserva versión anterior si existe.
- Tras descargar mundos y purgar caché no quedan retenciones propias inesperadas; counters distinguen caché evictable de fuga.

Verificación: V-CORE, V-ASSET.

Desbloquea: Importer y residencia GPU compartida.

## R03 — Game callbacks y primer SDK instalable externo

Fase: **P2** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias integradas: R02.

Locks: `runtime-core`, `public-api`, `session-bridge`, `build`.

Puntos de entrada: `include/vestigio/`, `src/api/`, `src/runtime/`, `src/session/session.c`, `src/player/main.c`, `cmake/`, `tests/sdk/`.

**Trabajo:**

1. Implementar init/world_ready/fixed_update/event/draw_ui/shutdown y stepping embebido; separar composición de juego de lifecycle en un cambio acotado.
2. Instalar SDK estático con headers y paquete CMake; exports explícitos de tooling al tocar su frontera, conservando ABI necesaria para Studio actual.
3. Crear consumidor C externo que cree mundo/cámara/entidades y lógica propia sin ReProject ni headers internos; backend GPU se conectará al integrar G03.

**Aceptación:**

- find_package y enlace desde fuera del repo funcionan sin Studio/.NET ni include de src; el ejemplo es un consumidor real, no target con acceso privilegiado al árbol.
- Orden/reentrancia de callbacks, init fallido, eventos limitados y shutdown parcial se prueban; no hay game DLL/hot reload implícitos.
- Juego legacy y pruebas de sesión siguen funcionando; diferencias de input se resuelven en I01, no con un segundo loop divergente.

Verificación: V-CORE, V-APP, V-SDK.

Desbloquea: Juego code-first sobre el mismo runtime.

## I01 — Acciones de input y settings comunes mínimos

Fase: **P2** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias integradas: R03.

Locks: `input-settings`, `session-bridge`, `public-api`, `platform-window`, `wpf-viewport`.

Puntos de entrada: `src/input/`, `src/config/`, `src/engine/input.c`, `src/platform/raylib_platform.c`, `src/player/main.c`, `src/studio/Controls/GameViewport.cs`, `src/studio/Native/`, `tests/input/`.

**Trabajo:**

1. Definir pressed/held/released por acción y adaptar Player/Studio al mismo acumulador de tick, con clear al perder foco.
2. Implementar esquema/defaults proyecto/usuario/sesión y validación; campos mínimos de video, bindings y sensibilidad, preservando extensión para audio/perfiles.
3. Guardar preferencias con versión y reemplazo seguro; declarar settings inmediatos o aplicables tras recreación de superficie.

**Aceptación:**

- Mantener tecla/ratón en Studio y Player genera acciones equivalentes a través de múltiples ticks; pausa/foco no deja acciones pegadas.
- Archivo corrupto/version futura/conflicto de binding tiene diagnóstico y no sobrescritura destructiva.
- Overrides de CLI/sesión no contaminan defaults persistidos; VSync/cap no alteran dt de simulación.

Verificación: V-CORE, V-APP, V-WPF.

Desbloquea: Controlador, preferencias y paridad entre hosts.

## D01 — Formato canónico y validadores de contenido

Fase: **P4** · Rol: **content** · Estado: **PLANNED**.

Dependencias integradas: R01.

Locks: `document-schema`.

Puntos de entrada: `src/content/`, `docs/formats/`, `tests/content/`, `docs/implementation/CONTRACTS.md`.

**Trabajo:**

1. Especificar e implementar parse/validate project/level JSON versionados usando un parser de procedencia revisada; fijar límites de lectura y tipos de componentes iniciales.
2. Definir UUIDs, referencias, TRS quaternion, environment, geometry legacy y extensiones; campos desconocidos optional se retienen y required se rechazan.
3. Añadir corpus de datos inválidos y diagnósticos con archivo/ruta/ID; no crear serializer C# paralelo ni representar todos los tipos futuros como implementados.

**Aceptación:**

- Validación rechaza duplicados, refs inexistentes, NaN/inf y versiones/capacidades incompatibles sin instanciar parcialmente mundo.
- Representación canónica de transforms/colores coincide con CONTRACTS y SDK; precisión y locale son explícitos.
- Round-trip estructural de componentes soportados y preservación de campos opcionales desconocidos tienen pruebas.

Verificación: V-CORE, V-DATA.

Desbloquea: Documento transaccional, migraciones y formatos de agentes.

## M01 — Importación GLB/glTF estática a representación propia

Fase: **P3** · Rol: **content** · Estado: **PLANNED**.

Dependencias integradas: R02.

Locks: `asset-import`.

Puntos de entrada: `src/assets/import/`, `tests/assets/fixtures/`, `THIRD_PARTY.md`.

**Trabajo:**

1. Integrar cgltf fijado con avisos correspondientes, sin exponer sus structs ni Model de raylib; parse, load buffers, validate y normalizar a IR propia.
2. Soportar malla indexada/no indexada, nodos/submeshes, UV/normales/texturas y materiales retro definidos; aplicar cambio de base Y-up a Z-up sin recenter de pivots.
3. Crear fixtures propias o con licencia registrada para GLB y glTF externo; declarar rechazo de skins/morphs/extensiones no soportadas en esta etapa.

**Aceptación:**

- Escala, ejes, jerarquía/pivot, winding, normales y alpha se validan con geometría de referencia numérica.
- Archivos truncados, índices/accesores fuera de rango y required extension no soportada devuelven error, sin crash ni lectura fuera de límite.
- Importar no necesita GPU; mismo source/opciones produce la misma IR y dependencias/fingerprint reproducibles.

Verificación: V-CORE, V-ASSET.

Desbloquea: Modelos 3D reales, sin parser glTF artesanal.

## G03 — Renderables y materiales GPU compartidos desde SDK

Fase: **P3** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias integradas: G01, R03, M01.

Locks: `gpu-backend`, `asset-gpu`, `public-api`.

Puntos de entrada: `src/render/`, `src/assets/`, `src/api/`, `tests/gpu/`, `tests/sdk/`.

**Trabajo:**

1. Conectar IR importada al registro de assets y backend; render packets mesh/material/transform/bounds, sprites y cámara desde World.
2. Añadir materiales básicos opaque/mask/blend y error material, culling/frustum y contadores; uploads únicos para meshes/texturas compartidas.
3. Extender consumidor C externo para cargar modelo y crear 100 instancias; destrucción y reload respetan versiones y contexto.

**Aceptación:**

- 100 instancias comparten mesh/textura; contador prueba una carga/upload por recurso residente, no una por entidad.
- Modelo y sprite delante/detrás, alpha y wireframe se verifican visualmente; renderer CPU no genera el frame final.
- Fallo de upload y descargar una instancia conservan las demás; dispose/purge libera recursos propios.

Verificación: V-APP, V-SDK, V-ASSET, V-GPU.

Desbloquea: Primer juego C con modelos GPU; asset browser y mundo mixto.

## G04 — Adaptador GPU de sectores y sprites legacy

Fase: **P3** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias integradas: G03.

Locks: `gpu-backend`, `legacy-adapter`, `session-bridge`.

Puntos de entrada: `src/render/`, `src/content/legacy/`, `src/session/session.c`, `src/engine/map.c`, `tests/gpu/`.

**Trabajo:**

1. Convertir sectores/portales/aberturas a mallas derivadas al cargar o editar, con invalidación por revisión; materiales de imagen y sprites actuales alimentan recursos nuevos.
2. Mantener reglas/colisión legacy en su contrato existente y geometría dinámica de barreras actualizada sin retessellar todo cada frame.
3. Comparar corpus Haunted/portales parciales en ambos backends por invariantes geométricas/visuales, no por igualdad pixel-perfect de shading.

**Aceptación:**

- Pisos, techos, portales parciales y huecos no quedan cerrados por mallas nuevas; sprites/atlas y materiales se conservan.
- Geometría estática no se sube por frame; editar una región invalida sólo datos necesarios según política documentada.
- Player usa el nuevo camino GPU con gameplay legacy preservado y opción de laboratorio CPU acotada.

Verificación: V-CORE, V-APP, V-GPU.

Desbloquea: Transición incremental sin perder niveles existentes.

## D02 — Documento nativo, transacciones y Tool API

Fase: **P4** · Rol: **content** · Estado: **PLANNED**.

Dependencias integradas: D01, R02.

Locks: `document-core`, `tool-api`.

Puntos de entrada: `src/content/`, `src/editor/editor.c`, `src/gameplay/transaction.c`, `include/vestigio/`, `tests/content/`.

**Trabajo:**

1. Implementar documento/instanciador con mismos constructores/validadores de componentes del runtime y assets por ID; mantener blobs y GPU fuera del historial.
2. Implementar begin(expected revision), comandos tipados, validate/commit/cancel, mapping de IDs y save transaccional recuperable.
3. Conservar semántica undo/redo actual, duplicación con remap y unknown fields; autosave separado del guardado manual.

**Aceptación:**

- Lote inválido no cambia documento ni destruye redo; conflicto de revisión no pisa edición concurrente.
- Crear/duplicar/reparentar/guardar/reabrir conserva campos y refs; asset pesado no se copia por cada undo.
- Fallo entre archivos de commit tiene recuperación reproducible y no sobrescribe una versión futura.

Verificación: V-CORE, V-DATA.

Desbloquea: Editor y automatización sobre una autoridad documental.

## D03 — Migración legacy y round-trip de proyecto completo

Fase: **P4** · Rol: **content** · Estado: **PLANNED**.

Dependencias integradas: D02, G04.

Locks: `document-core`, `legacy-adapter`.

Puntos de entrada: `src/content/legacy/`, `src/gameplay/project.c`, `tests/content/`, `assets/studio/`.

**Trabajo:**

1. Implementar migración sobre copia de mapas/proyecto/actores/reglas/diálogos; generar UUID determinístico y preservar portales/etiquetas/referencias.
2. Resolver catálogo y subrecursos importados sin depender de cwd; contenido legado se representa como adaptador, no reinterpretación destructiva.
3. Probar proyecto migrado mediante instanciador y GPU; no sobrescribir assets de ejemplo originales durante pruebas.

**Aceptación:**

- Haunted y fixtures de portales/reglas migran, se guardan, reabren y juegan sin pérdida semántica declarada.
- Mover proyecto a ruta con espacios/Unicode conserva resolución de recursos; referencia perdida informa ID/ruta.
- Migración repetida es estable y conserva originales; versión nueva no se escribe como si fuera formato legacy.

Verificación: V-CORE, V-APP, V-DATA, V-GPU.

Desbloquea: Base de contenido migrable y utilizable.

## E01 — Viewport editorial GPU conectado al documento

Fase: **P5** · Rol: **editor** · Estado: **PLANNED**.

Dependencias integradas: G02, G03, D02, I01.

Locks: `wpf-viewport`, `tool-api`.

Puntos de entrada: `src/studio/Controls/`, `src/studio/Native/`, `src/studio/Models/EditorDocument.cs`, `src/editor/`, `tests/studio/`.

**Trabajo:**

1. Integrar superficie aprobada en G02 con EditWorld, cámara libre/orbit/ortográfica, frame seleccionado y renderables de G03.
2. Actualizar EditWorld desde revisiones/eventos del documento; Play crea copia aislada y Stop vuelve a edición sin guardar mutaciones de juego.
3. Implementar selección por UUID y picking editorial por ray/bounds con máscara para luces/triggers/cámaras, independiente de colisión de gameplay.

**Aceptación:**

- Abrir documento/importar recurso y verlo en viewport comparte runtime/validadores con consumidor C.
- Editar cámara/selección no marca contenido dirty; Play/Stop no modifica documento ni partida del usuario.
- Foco, held input, resize y GPU sin readback normal siguen pasando al integrarse con Studio real.

Verificación: V-APP, V-GPU, V-WPF, V-DATA.

Desbloquea: Edición directa sobre la escena real.

## E02 — Gizmos, multiselección y comandos de transformación

Fase: **P5** · Rol: **editor** · Estado: **PLANNED**.

Dependencias integradas: E01.

Locks: `wpf-viewport`, `document-commands`.

Puntos de entrada: `src/studio/Controls/`, `src/editor/`, `src/render/overlays/`, `tests/studio/`, `tests/content/`.

**Trabajo:**

1. Implementar move/rotate/scale, local/world, pivots y snapping compartido; preview efímero con commit al terminar gesto.
2. Añadir duplicar/borrar/reparentar en lote con UUIDs/remap; validar parent cycles/shear antes del commit.
3. Dibujar gizmos GPU y mantener selección válida al cambiar índices/revisión; Escape cancela y un drag equivale a un undo.

**Aceptación:**

- Undo/redo restaura gesto y refs completos; fallo o Escape no borra redo ni ensucia documento.
- Multiselección y pivots producen mismo resultado por Tool API y UI, incluida jerarquía rotada.
- Guardar/reabrir reproduce transforms; tests de datos más revisión visual real del gizmo.

Verificación: V-CORE, V-DATA, V-WPF.

Desbloquea: Autoría de objetos con edición fiable.

## E03 — Inspector, jerarquía y biblioteca de assets

Fase: **P5** · Rol: **editor** · Estado: **PLANNED**.

Dependencias integradas: E02, M01.

Locks: `wpf-inspector`, `document-commands`.

Puntos de entrada: `src/studio/ViewModels/StudioViewModel.cs`, `src/studio/Models/`, `src/studio/MainWindow.xaml`, `src/studio/Services/`, `src/editor/`, `tests/studio/`.

**Trabajo:**

1. Generar campos desde esquemas tipados con unidades/rangos/referencias y mensajes de error; multiedición conserva valores no modificados.
2. Separar layers/grupos editoriales de parent runtime; browser importa, coloca y muestra estado/diagnóstico/fingerprint de recursos.
3. Añadir flujo importación-modelo a documento con preview/thumbnail y placeholder, sin que vista sea propietaria de la textura compartida.

**Aceptación:**

- Importar→colocar→transformar→guardar→reabrir recupera modelo/material/IDs sin duplicar recursos.
- Renombrar/reimportar recursos mantiene identidad prevista; errores se muestran junto al campo y en Problems/log.
- Ocultar capa de editor no altera visibilidad de juego; propiedades soportadas tienen round-trip y una acción undo por lote.

Verificación: V-APP, V-ASSET, V-DATA, V-WPF.

Desbloquea: Flujo editorial de contenido completo.

## E04 — Herramientas de habitaciones, aberturas y organización

Fase: **P5** · Rol: **editor** · Estado: **PLANNED**.

Dependencias integradas: E03, D03, S01.

Locks: `wpf-viewport`, `document-commands`, `legacy-adapter`.

Puntos de entrada: `src/editor/`, `src/content/`, `src/studio/Controls/`, `tests/content/`, `tests/studio/`.

**Trabajo:**

1. Añadir recetas acotadas de habitación/pared y abertura con preview; conservar herramientas sectoriales existentes y adaptar geometría derivada.
2. Implementar grid/cotas/capas/plantas como organización de autoría y ghosting opcional; no ocultar plantas automáticamente en runtime.
3. Conectar operaciones al mismo historial/validadores y recalcular bounds/malla/collider invalidado; evitar construir un modelador 3D general.

**Aceptación:**

- Crear habitación y abertura, deshacer/rehacer, guardar/reabrir mantiene geometría y portales.
- Receta es autoridad y mesh derivada se regenera sin duplicación divergente; no hay hueco visual con collider viejo al integrarse con S01.
- Preview/cancel no deja objetos huérfanos; edificio con atrio no pierde pisos visibles por regla editorial.

Verificación: V-CORE, V-DATA, V-WPF, V-GPU.

Desbloquea: World building recuperado de js-game sin portar sus errores.

## S01 — Consultas espaciales 3D y colliders compartidos

Fase: **P6** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias integradas: G03, D02.

Locks: `spatial-core`, `public-api`.

Puntos de entrada: `src/physics/`, `src/world/`, `src/api/`, `tests/spatial/`.

**Trabajo:**

1. Implementar raycast/sweep/overlap con máscaras, ignored entity y resultados entidad/normal/distancia/fracción; static mesh con aceleración acotada y colliders dinámicos simples.
2. Mantener fuente de geometría/collider/transform común con runtime; adaptar consultas legacy sin exigir BSP/navmesh nuevos a todos los niveles.
3. Definir escala/colliders admitidos, límites numéricos y actualización tras transform/edición; no simular rigid bodies generales.

**Aceptación:**

- Ray/movimiento/visión/proyectil usan máscaras coherentes y no atraviesan props sólidos por falta silenciosa de collider.
- Pruebas reproducen contacto tangencial, esquina, malla fina, rotación/padre y escala admitida con tolerancias documentadas.
- Editar/reimportar collider invalida estructura espacial; overlays muestran geometría de colisión real.

Verificación: V-CORE, V-SPATIAL.

Desbloquea: Espacio 3D jugable y picking preciso opcional.

## S02 — Controlador 3D y navegación mínima

Fase: **P6** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias integradas: S01, I01.

Locks: `spatial-core`, `gamekit`, `session-bridge`.

Puntos de entrada: `src/gamekit/`, `src/physics/`, `src/session/session.c`, `tests/spatial/`, `tests/gamekit/`.

**Trabajo:**

1. Construir controlador cinemático de cápsula/volumen admitido con suelo, pendiente, escalón, techo y dt fijo; configuración expuesta al SDK.
2. Integrar cámara/jugador opt-in sin asumir armas; preservar navegación BFS legacy y añadir waypoints/consultas mínimos para actores fuera de sectores.
3. Crear recorrido de prueba con props, escalera y pasillo estrecho; registrar límites admitidos y comportamiento ante penetración inicial.

**Aceptación:**

- Movimiento estable con render a distintas frecuencias; no atraviesa suelo/techo/esquinas bajo velocidad máxima admitida.
- Jugador cabe/no cabe conforme a volumen, pendiente/escalón válidos y plataformas definidas; FPS no es dependencia del core.
- Actor legacy conserva recorrido; navegación 3D no afirma cobertura navmesh que no existe.

Verificación: V-CORE, V-APP, V-SPATIAL.

Desbloquea: Exploración 3D y colisión de puertas.

## S03 — Puertas con bisagra, triggers e interacción

Fase: **P6** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias integradas: S02, D02, R03.

Locks: `gamekit`, `document-schema`.

Puntos de entrada: `src/gamekit/`, `src/gameplay/interaction.c`, `src/content/`, `tests/gamekit/`, `tests/spatial/`.

**Trabajo:**

1. Implementar raíz/bisagra/panel con eje/pivot/ángulos/speed, estados closed/opening/open/closing/blocked/locked y obstrucción segura.
2. Mover collider con misma transform del panel, incluso totalmente abierto; sweep angular/substeps con límites derivados de dimensiones/velocidad.
3. Conectar key/lock/auto-close, Interactable y Trigger enter/exit/once/cooldown a eventos/reglas; emitir eventos de audio, reproducidos al integrar A01.

**Aceptación:**

- Puerta gira en ambos sentidos y con padre rotado; cierre sobre cuerpo bloquea/revierte sin aplastar por defecto ni tunneling dentro de límites probados.
- Raycast/visión/proyectil/movimiento coinciden con panel y collider abierto sigue sólido.
- Definición de puerta/triggers sobrevive round-trip; estado mutable serializable queda listo para P01. Sonido sólo se acepta como conectado después de A01/E05.

Verificación: V-CORE, V-DATA, V-SPATIAL.

Desbloquea: Interacción física de horror/exploración.

## V01 — Materiales, shaders editables, luces y fog GPU

Fase: **P7** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias integradas: G03, I01, D02.

Locks: `gpu-backend`, `shader-schema`.

Puntos de entrada: `src/render/`, `src/assets/`, `src/content/`, `assets/shaders/`, `tests/gpu/`.

**Trabajo:**

1. Implementar ShaderAsset/material params tipados y metadata de inspector; reload candidato con dependencias y log de compilación.
2. Añadir forward simple, luces por draw limitadas/priorizadas y fog lineal/exp con distancia definida, alpha y sky coherentes.
3. Registrar light/material/environment schemas para documento/SDK sin escribir UI paralela; budgets/overlays muestran límites excedidos.

**Aceptación:**

- Shader inválido conserva el anterior; primer fallo tiene error material/diagnóstico explícito y no mata sesión.
- Luces/fog se ejecutan en GPU y sus parámetros sobreviven save/load; no se atribuyen sombras geométricas inexistentes.
- Entradas/espacio de color/unidades y límites son probados; no se incorpora PBR/volumétricos como dependencia del hito.

Verificación: V-APP, V-GPU, V-ASSET, V-DATA.

Desbloquea: Estilo visual editable sin bifurcar renderer.

## V02 — Perfiles Neo-PSX, upscale y settings de vídeo

Fase: **P7** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias integradas: V01.

Locks: `gpu-backend`, `input-settings`, `shader-schema`.

Puntos de entrada: `src/render/`, `src/config/`, `assets/shaders/`, `tests/gpu/`, `tests/config/`.

**Trabajo:**

1. Implementar presets clean/retro/PSX/custom con snapping, UV affine opcional, Bayer/dither y cuantización después de fog/lighting a resolución interna.
2. Resolver ping-pong postprocess, HUD pixelado/nítido, filtro, escala entera/fraccional/aspecto, fullscreen/borderless y rollback de targets/settings.
3. Separar capacidades de backend de perfil; ningún perfil GPU fuerza readback ni exige paridad de custom shaders con renderer CPU.

**Aceptación:**

- Cambiar perfil en el mismo nivel funciona; snapping cerca del near plane no rompe clipping y affine no es un efecto de ruido de pantalla.
- 320x180/426x240/640x360 conservan aspecto en resize/DPI; barras y dither permanecen ligados a píxel interno.
- Settings persisten con overrides correctos, targets viejos se liberan y efectos pueden desactivarse sin editar assets.

Verificación: V-CORE, V-APP, V-GPU, V-DATA.

Desbloquea: Neo-PSX como perfil del mismo runtime.

## A01 — Audio de proyecto, voces, música y emisores

Fase: **P7** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias integradas: R02, I01, D02.

Locks: `audio-core`, `public-api`.

Puntos de entrada: `src/audio/`, `src/assets/`, `src/platform/raylib_platform.c`, `src/api/`, `src/content/`, `tests/audio/`.

**Trabajo:**

1. Separar Sound compartido, Voice mutable y Music streaming; buses master/music/SFX/ambience y listener/emisor espacial.
2. Conectar eventos de interacción al playback sin repetir sonidos por frame; límites de voces y política de prioridad explícitos.
3. Exponer configuración al SDK y esquema documental, lifecycle de streaming y pausa/foco/cambio de mundo; registrar contratos reales de pan/volumen de backend.

**Aceptación:**

- Dos emisores comparten Sound pero tienen voces independientes; descargar mundo detiene sus voces sin invalidar otros usuarios.
- Música se actualiza sin cargar todo como PCM y buses persisten; dispositivo ausente/fallo se comunica sin crash.
- Escucha real confirma posición/volumen/evento y pausa; pruebas automatizadas cubren ownership y límites, no sustituyen audición.

Verificación: V-CORE, V-APP, V-ASSET, V-AUDIO, V-DATA.

Desbloquea: Puertas audibles, ambiente y música de juego.

## A02 — Animación rígida y skeletal con pose por instancia

Fase: **P7** · Rol: **content** · Estado: **PLANNED**.

Dependencias integradas: G03, D02.

Locks: `asset-import`, `gpu-skinning`, `gpu-backend`, `document-schema`.

Puntos de entrada: `src/assets/import/`, `src/world/`, `src/render/`, `src/content/`, `tests/assets/`, `tests/gpu/`.

**Trabajo:**

1. Extender importer/IR con clips de nodos y después skins/joints/inverse bind matrices; definir interpolaciones soportadas y rechazar el resto explícitamente.
2. Separar asset de animación/mesh compartido de tiempo/pose por instancia; skinning GPU dentro de capacidades/budgets definidos.
3. Mantener billboards/atlas legacy y serializar parámetros de instancia; coordinar layout de atributos/uniforms con responsable GPU antes de modificar backend.

**Aceptación:**

- Dos instancias del mismo asset animan con tiempos diferentes sin duplicar mesh ni pose compartida accidental.
- Pose de reposo/ejes/normales y jerarquía coinciden con fixtures numéricas/visuales; error de skin inválido no publica recurso parcial.
- GPU ejecuta skinning en perfil soportado, recursos se liberan y clip/velocidad/default persisten; morph targets no se anuncian si no se implementan.

Verificación: V-CORE, V-ASSET, V-GPU, V-DATA.

Desbloquea: Personajes/modelos animados reales.

## E05 — Cerrar el recorrido editorial completo

Fase: **P5-P7** · Rol: **editor** · Estado: **PLANNED**.

Dependencias integradas: E04, S03, V02, A01, A02, D03.

Locks: `wpf-inspector`, `wpf-viewport`, `tool-api`.

Puntos de entrada: `src/studio/`, `src/editor/`, `tests/studio/`, `tests/journeys/`.

**Trabajo:**

1. Integrar campos/gizmos de luz/fog/material/shader/audio/trigger/interactable/puerta/spawn/actor/cámara/animación sobre esquemas reales; bridge para tipos de juego registrados.
2. Cerrar crear habitación→importar modelo→poner puerta/luces/audio/trigger→guardar→cerrar→reabrir→jugar→detener; incluir undo y recuperación.
3. Sincronizar colliders de edición, mensajes de validación, logs y debug overlays; verificar accesibilidad de teclado, DPI/docking y separación de Play.

**Aceptación:**

- Todos los datos editables del recorrido sobreviven round-trip y producen comportamiento en Player, incluida puerta con audio y bloqueo físico.
- Cambios de juego durante preview no alteran documento/partida; selección y foco vuelven correctamente al detener.
- Evidencia visual y ejecución integrada verifican UI/runtime/archivo; un widget o screenshot aislado no cierra el ticket.

Verificación: V-APP, V-WPF, V-DATA, V-SPATIAL, V-AUDIO, V-GPU.

Desbloquea: Creator 3D completo según alcance confirmado.

## Q01 — Instrumentación y presupuesto de rendimiento medido

Fase: **P7-P8** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias integradas: E05.

Locks: `profiling`, `gpu-backend`.

Puntos de entrada: `src/debug/`, `src/render/`, `src/runtime/`, `tests/perf/`, `docs/implementation/evidence/`.

**Trabajo:**

1. Consolidar consola/overlays, CPU simulation/prepare/submit/present, GPU por pass cuando soportado, p50/p95/p99, draw calls, uploads/readbacks, entidades/assets y RAM/VRAM estimada.
2. Medir corpus integrado en hardware registrado, con warmup/duración/configuración reproducible; separar integrado/dedicado si se dispone de ambos.
3. Optimizar sólo cuellos medidos y revalidar; fijar límites operativos y degradación explícita de luces/efectos, sin inventar 60 FPS garantizados.

**Aceptación:**

- Reportes distinguen estimación de memoria de medición driver y coste CPU de GPU; queries de tiempo no bloquean cada frame.
- Recargar mundo/shader/assets repetidamente no presenta crecimiento sostenido de recursos propios; capturas no contaminan cifras del frame normal.
- Presupuesto aceptado y limitaciones quedan documentados para hardware realmente probado; ausencia de otro equipo queda pendiente explícita.

Verificación: V-APP, V-GPU, V-PERF.

Desbloquea: Criterio de rendimiento de producto con evidencia.

## P01 — Savegame versionado por componente

Fase: **P8** · Rol: **content** · Estado: **PLANNED**.

Dependencias integradas: S03, A01, A02, R03, D02.

Locks: `savegame`, `document-core`, `gamekit`.

Puntos de entrada: `src/content/`, `src/gamekit/`, `src/gameplay/interaction.c`, `src/session/session.c`, `tests/content/`, `tests/gamekit/`.

**Trabajo:**

1. Separar nivel/autosave/partida y definir persistencia de jugador, puertas, actores, variables, inventario, RNG, triggers y estado de módulo C.
2. Resolver transitorios: proyectiles/timers/animación/música se persisten o reinician según contrato explícito; nunca snapshot de punteros/memoria arbitraria.
3. Restaurar candidato contra ProjectId/LevelId/content revision con migración o error; guardar de forma recuperable.

**Aceptación:**

- Guardar a mitad de apertura y restaurar conserva ángulo/lock/timer admitidos; actores/variables y transitorios siguen política declarada.
- Save corrupto/incompatible no aplica estado a otras entidades ni destruye partida anterior.
- Juego C puede aportar estado versionado sin modificar core; lifecycle de recursos/voces tras restore queda verificado.

Verificación: V-CORE, V-APP, V-DATA, V-SPATIAL, V-AUDIO.

Desbloquea: Persistencia de partida y entrega independiente.

## P02 — Dos juegos y exportación independiente del repositorio

Fase: **P8** · Rol: **integrator** · Estado: **PLANNED**.

Dependencias integradas: E05, P01, Q01.

Locks: `integration`, `build`, `public-api`, `packaging`.

Puntos de entrada: `examples/`, `cmake/`, `tools/export-project.ps1`, `CMakeLists.txt`, `THIRD_PARTY.md`, `tests/sdk/`, `tests/export/`.

**Trabajo:**

1. Crear FPS/editor y exploración C sin armas con mismo runtime; registrar tipos/acciones propios visibles en Studio y generar habitación/entidades/luces por Tool API.
2. Completar SDK instalable y build/export por cierre transitivo de dependencias, avisos, host y game module compilado; excluir caché/src/autosaves salvo inclusión explícita.
3. Probar paquetes fuera del repo con cwd arbitrario/rutas Unicode; preservar mejoras previas del exportador y evitar borrados fuera de destino verificado.

**Aceptación:**

- Ambos juegos se construyen/juegan sin tocar internals y Player exportado no requiere Studio/.NET ni carpetas del desarrollador.
- Dependencia ausente impide export con diagnóstico; ningún asset queda omitido por depender sólo de strings no declarados.
- Inventario/licencias de código y fixtures permite distribuir lo seleccionado; licencia del código propio no se inventa: la decisión del titular se registra antes de publicación pública.

Verificación: V-CORE, V-APP, V-SDK, V-DATA, V-DELIVERY.

Desbloquea: Prueba de engine/SDK utilizable por dos juegos distintos.

## T01 — CLI sobre Tool API con cambios revisables

Fase: **P8** · Rol: **content** · Estado: **PLANNED**.

Dependencias integradas: P02.

Locks: `tool-api`, `cli`.

Puntos de entrada: `src/cli/`, `src/editor/`, `tests/cli/`, `docs/`.

**Trabajo:**

1. Implementar project/level validate, asset inspect/import, level apply y build mediante servicios existentes; códigos de salida y JSON de diagnósticos documentados.
2. Ofrecer dry-run, expected revision, lote atómico y mapping de IDs para generación procedural; no ejecutar C arbitrario para validar datos.
3. Añadir ejemplo de automatización que genere contenido abrible/editable por Studio; no implementar servidor MCP, RPC o binding extra en este ticket.

**Aceptación:**

- Mismo lote aplicado por CLI y Studio produce contenido equivalente; conflicto deja archivos intactos.
- Validaciones/documentos funcionan sin GPU; comandos que requieren render declaran capacidad y no fallan de forma opaca en headless.
- Comandos y errores se prueban como procesos reales con paths espacios/Unicode y códigos de salida útiles.

Verificación: V-CORE, V-DATA, V-DELIVERY.

Desbloquea: Automatización por agentes sobre un contrato estable.

## Z01 — Aceptación integrada y cierre de implementación

Fase: **P8** · Rol: **integrator** · Estado: **PLANNED**.

Dependencias integradas: P02, T01.

Locks: `integration`, `build`, `packaging`.

Puntos de entrada: `docs/implementation/STATE.md`, `docs/implementation/evidence/`, `docs/`, `tests/`.

**Trabajo:**

1. Construir candidato único con todos los tickets integrados y ejecutar matriz completa y recorridos editor-first/code-first/export/restore.
2. Auditar backlog/contratos/documentación frente a implementación, marcar límites reales, registrar revisiones de evidencia y defectos abiertos.
3. Entregar instrucciones de uso/SDK/build/diagnóstico, artifacts y rollback; no desplegar/publicar remotamente por el solo hecho de completar el plan.

**Aceptación:**

- Gates de VALIDATION pasan en el candidato integrado o el producto permanece explícitamente incompleto; no sumar PASS de ramas incompatibles.
- No quedan requisitos obligatorios representados por stubs, tests omitidos o simples capturas; aceptación humana se registra separada si no disponible.
- Resumen final identifica qué se implementó, pruebas reales, hardware, límites, archivos/commits y trabajo restante.

Verificación: V-CORE, V-APP, V-SDK, V-GPU, V-WPF, V-ASSET, V-DATA, V-SPATIAL, V-AUDIO, V-PERF, V-DELIVERY.

Desbloquea: Cierre verificable del alcance; extensiones futuras quedan fuera.
