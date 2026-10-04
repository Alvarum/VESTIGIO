# Tickets de implementación

Generado desde [backlog.json](backlog.json). Editar el JSON y ejecutar `python docs/implementation/validate-plan.py --render`; no mantener dos versiones manuales.

Z01 cierra el alcance original de 30 tickets. ZA1 cierra la ampliación derivada de `js-game`. Los tickets nuevos siguen siendo propuestas individuales; su presencia no autoriza ejecutarlos en bloque.

Los paths son puntos de entrada; directorios nuevos son propuestas hasta F01. Antes de escribir se reclama una lista exacta de archivos. Los perfiles se definen en [VALIDATION.md](VALIDATION.md).

## F00 — Reconciliar baseline y obtener evidencia inicial

Fase: **P0** · Rol: **integrator** · Estado: **INTEGRATED**.

Dependencias: ninguna; inicio del plan.

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

Fase: **P0** · Rol: **integrator** · Estado: **INTEGRATED**.

Dependencias: F00.

Locks: `public-api`, `build`, `contracts`.

Puntos de entrada: `docs/implementation/CONTRACTS.md`, `engines/vestigio/include/vestigio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/src/render/`, `engines/vestigio/CMakeLists.txt`, `engines/vestigio/tests/sdk/`.

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

Commit integrado: `2052e5b4d69f8ae169df073f3ed8a761b80c62dc`.

## G01 — Primer frame de geometría GPU en ventana propia

Fase: **P1** · Rol: **gpu** · Estado: **INTEGRATED**.

Dependencias: F01.

Locks: `gpu-backend`, `platform-window`.

Puntos de entrada: `engines/vestigio/src/render/gpu_raylib/`, `engines/vestigio/tests/gpu/`.

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

Evidencia: docs/implementation/evidence/G01.md.

Commit integrado: `a13fb8beb89691d842709773abadb0b755d8f9a6`.

## R01 — Contexto, mundo, entidades y Transform con handles

Fase: **P2** · Rol: **runtime** · Estado: **INTEGRATED**.

Dependencias: F01.

Locks: `runtime-core`, `public-api`.

Puntos de entrada: `engines/vestigio/include/vestigio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/src/world/`, `engines/vestigio/tests/runtime/`.

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

Evidencia: docs/implementation/evidence/R01.md.

Commit integrado: `9ed4d5e`.

## G02 — Resolver superficie GPU de Studio mediante spike

Fase: **P1** · Rol: **gpu** · Estado: **INTEGRATED**.

Dependencias: G01.

Locks: `gpu-backend`, `platform-window`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/Controls/`, `engines/vestigio/studio/Native/`, `engines/vestigio/src/platform/`, `engines/vestigio/studio.tests/`, `docs/implementation/CONTRACTS.md`.

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

Evidencia: docs/implementation/evidence/G02.md.

Commit integrado: `9811b03`.

## R02 — Registro de assets y ownership CPU/GPU mínimo

Fase: **P2** · Rol: **runtime** · Estado: **INTEGRATED**.

Dependencias: R01.

Locks: `asset-core`, `public-api`.

Puntos de entrada: `engines/vestigio/src/assets/`, `engines/vestigio/include/vestigio/`, `engines/vestigio/tests/assets/`.

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

Evidencia: docs/implementation/evidence/R02.md.

Commit integrado: `9ed4d5e`.

## R03 — Game callbacks y primer SDK instalable externo

Fase: **P2** · Rol: **runtime** · Estado: **INTEGRATED**.

Dependencias: R02.

Locks: `runtime-core`, `public-api`, `session-bridge`, `build`.

Puntos de entrada: `engines/vestigio/include/vestigio/`, `engines/vestigio/src/api/`, `engines/vestigio/src/runtime/`, `engines/vestigio/cmake/`, `engines/vestigio/tests/sdk/`.

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

Evidencia: docs/implementation/evidence/R03.md.

Commit integrado: `df05608`.

## I01 — Acciones de input y settings comunes mínimos

Fase: **P2** · Rol: **runtime** · Estado: **INTEGRATED**.

Dependencias: R03.

Locks: `input-settings`, `session-bridge`, `public-api`, `platform-window`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/src/input/`, `engines/vestigio/src/config/`, `engines/vestigio/studio/Native/`, `engines/vestigio/tests/input/`.

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

Evidencia: docs/implementation/evidence/I01.md.

Commit integrado: `683ad2d`.

## D01 — Formato canónico y validadores de contenido

Fase: **P4** · Rol: **content** · Estado: **INTEGRATED**.

Dependencias: R01.

Locks: `document-schema`.

Puntos de entrada: `engines/vestigio/src/content/`, `docs/formats/`, `engines/vestigio/tests/content/`, `docs/implementation/CONTRACTS.md`.

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

Evidencia: docs/implementation/evidence/D01.md.

Commit integrado: `1acbd03`.

## M01 — Importación GLB/glTF estática a representación propia

Fase: **P3** · Rol: **content** · Estado: **INTEGRATED**.

Dependencias: R02.

Locks: `asset-import`.

Puntos de entrada: `engines/vestigio/src/assets/import/`, `engines/vestigio/tests/assets/fixtures/`, `THIRD_PARTY.md`.

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

Evidencia: docs/implementation/evidence/M01.md.

Commit integrado: `656fda7`.

## G03 — Renderables y materiales GPU compartidos desde SDK

Fase: **P3** · Rol: **gpu** · Estado: **INTEGRATED**.

Dependencias: G01, R03, M01.

Locks: `gpu-backend`, `asset-gpu`, `public-api`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/assets/`, `engines/vestigio/src/api/`, `engines/vestigio/tests/gpu/`, `engines/vestigio/tests/sdk/`.

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

Evidencia: docs/implementation/evidence/G03.md.

Commit integrado: `50ece2a`.

## J01 — Primera escena 3D nueva y recorrible en GPU

Fase: **P3** · Rol: **runtime** · Estado: **INTEGRATED**.

Dependencias: G03, I01, D02.

Locks: `platform-window`, `session-bridge`, `public-api`.

Puntos de entrada: `engines/vestigio/src/player/`, `engines/vestigio/src/api/`, `engines/vestigio/examples/`, `engines/vestigio/tests/sdk/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Crear un proyecto de ejemplo nuevo con suelo visual, algunos objetos y un modelo glTF/GLB importado; no convertir ni migrar Haunted.
2. Conectar el consumidor de SDK al host Player y a sus acciones de input para recorrer la escena con cámara en GPU; si falta una pieza pública, añadir sólo la conexión imprescindible.
3. Documentar comando de arranque y controles; comprobar apertura, recorrido, resize y cierre con una prueba dirigida y una captura en GPU real.

**Aceptación:**

- Un usuario arranca una escena nueva desde un comando reproducible, ve el modelo importado y puede desplazar la cámara; el frame final se dibuja en GPU.
- El ejemplo usa el SDK público sin incluir internals; Player libera ventana y recursos al cerrar, con errores legibles si falta el asset.
- Este hito es recorrido libre sin colisiones ni editor; no se anuncia como juego completo.

Verificación: V-APP, V-SDK, V-GPU.

Desbloquea: Demo mínima visible antes de financiar física o autoría.

Evidencia: docs/implementation/evidence/J01.md.

Commit integrado: `7efc54c`.

## D02 — Documento nativo, transacciones y Tool API

Fase: **P4** · Rol: **content** · Estado: **INTEGRATED**.

Dependencias: D01, R02.

Locks: `document-core`, `tool-api`.

Puntos de entrada: `engines/vestigio/src/content/`, `engines/vestigio/include/vestigio/`, `engines/vestigio/tests/content/`.

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

Evidencia: docs/implementation/evidence/D02.md.

Commit integrado: `2e15e5e`.

## E01 — Viewport editorial GPU conectado al documento

Fase: **P5** · Rol: **editor** · Estado: **INTEGRATED**.

Dependencias: G02, G03, D02, I01.

Locks: `wpf-viewport`, `tool-api`.

Puntos de entrada: `engines/vestigio/studio/Controls/`, `engines/vestigio/studio/Native/`, `engines/vestigio/src/editor/`, `engines/vestigio/studio.tests/`.

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

Evidencia: docs/implementation/evidence/E01.md.

Commit integrado: `73fe637`.

## E02 — Gizmos, multiselección y comandos de transformación

Fase: **P5** · Rol: **editor** · Estado: **INTEGRATED**.

Dependencias: E01.

Locks: `wpf-viewport`, `document-commands`.

Puntos de entrada: `engines/vestigio/studio/Controls/`, `engines/vestigio/src/editor/`, `engines/vestigio/src/render/overlays/`, `engines/vestigio/studio.tests/`, `engines/vestigio/tests/content/`.

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

Evidencia: docs/implementation/evidence/W04.md, docs/implementation/evidence/E02.md.

Commit integrado: `c3f475206f9c770f79b074037dc28690b05df387`.

## E03 — Inspector, jerarquía y biblioteca de assets

Fase: **P5** · Rol: **editor** · Estado: **INTEGRATED**.

Dependencias: E02, M01.

Locks: `wpf-inspector`, `document-commands`.

Puntos de entrada: `engines/vestigio/studio/Models/`, `engines/vestigio/studio/Services/`, `engines/vestigio/src/editor/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Generar campos desde esquemas tipados con unidades/rangos/referencias y mensajes de error; multiedición conserva valores no modificados.
2. Separar layers/grupos editoriales de parent runtime; browser importa, coloca y muestra estado/diagnóstico/fingerprint de recursos.
3. Añadir flujo importación-modelo a documento con preview/thumbnail y placeholder, sin que vista sea propietaria de la textura compartida.

**Aceptación:**

- Importar→colocar→transformar→guardar→reabrir recupera modelo/material/IDs sin duplicar recursos.
- Renombrar/reimportar recursos mantiene identidad prevista; errores se muestran junto al campo y en Problems/log.
- Ocultar capa de editor no altera visibilidad de juego; propiedades soportadas tienen round-trip y una acción undo por lote.
- El catalogo conserva referencias estables al archivo GLB; el objeto colocado mantiene transform y material tras guardar, cerrar, abrir y jugar. La seleccion de nodos/submodelos corresponde a H02.

Verificación: V-APP, V-ASSET, V-DATA, V-WPF.

Desbloquea: Flujo editorial de contenido completo.

Evidencia: docs/implementation/evidence/E03.md.

Commit integrado: `69f9e81`.

## E04 — Herramientas de habitaciones, aberturas y organización

Fase: **P5** · Rol: **editor** · Estado: **INTEGRATED**.

Dependencias: E03, S01.

Locks: `wpf-viewport`, `document-commands`.

Puntos de entrada: `engines/vestigio/src/editor/`, `engines/vestigio/src/content/`, `engines/vestigio/studio/Controls/`, `engines/vestigio/tests/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Añadir recetas nuevas y acotadas de habitación/pared y abertura con preview y geometría derivada.
2. Implementar grid/cotas/capas/plantas como organización de autoría y ghosting opcional; no ocultar plantas automáticamente en runtime.
3. Conectar operaciones al mismo historial/validadores y recalcular bounds/malla/collider invalidado; evitar construir un modelador 3D general.

**Aceptación:**

- Crear habitación y abertura, deshacer/rehacer, guardar/reabrir mantiene geometría y portales.
- Receta es autoridad y mesh derivada se regenera sin duplicación divergente; no hay hueco visual con collider viejo al integrarse con S01.
- Preview/cancel no deja objetos huérfanos; edificio con atrio no pierde pisos visibles por regla editorial.

Verificación: V-CORE, V-DATA, V-WPF, V-GPU.

Desbloquea: World building recuperado de js-game sin portar sus errores.

Evidencia: docs/implementation/evidence/E04.md.

Commit integrado: `7048a59e0d79f6f34a34828bd3d98fc945c43a77`.

## S01 — Consultas espaciales 3D y colliders compartidos

Fase: **P6** · Rol: **runtime** · Estado: **INTEGRATED**.

Dependencias: G03, D02.

Locks: `spatial-core`, `public-api`.

Puntos de entrada: `engines/vestigio/src/physics/`, `engines/vestigio/src/world/`, `engines/vestigio/src/api/`, `engines/vestigio/tests/spatial/`.

**Trabajo:**

1. Implementar raycast/sweep/overlap con máscaras, ignored entity y resultados entidad/normal/distancia/fracción; static mesh con aceleración acotada y colliders dinámicos simples.
2. Mantener fuente de geometría/collider/transform común con runtime y la escena nueva; no exigir BSP/navmesh a los niveles nuevos.
3. Definir escala/colliders admitidos, límites numéricos y actualización tras transform/edición; no simular rigid bodies generales.

**Aceptación:**

- Ray/movimiento/visión/proyectil usan máscaras coherentes y no atraviesan props sólidos por falta silenciosa de collider.
- Pruebas reproducen contacto tangencial, esquina, malla fina, rotación/padre y escala admitida con tolerancias documentadas.
- Editar/reimportar collider invalida estructura espacial; overlays muestran geometría de colisión real.

Verificación: V-CORE, V-SPATIAL.

Desbloquea: Espacio 3D jugable y picking preciso opcional.

Evidencia: docs/implementation/evidence/S01.md.

Commit integrado: `22e937a2d589e256f38ff7268603c26723343cae`.

## S02 — Controlador 3D y navegación mínima

Fase: **P6** · Rol: **runtime** · Estado: **INTEGRATED**.

Dependencias: S01, I01.

Locks: `spatial-core`, `gamekit`, `session-bridge`.

Puntos de entrada: `engines/vestigio/src/gamekit/`, `engines/vestigio/src/physics/`, `engines/vestigio/tests/spatial/`, `engines/vestigio/tests/gamekit/`.

**Trabajo:**

1. Construir controlador cinemático de cápsula/volumen admitido con suelo, pendiente, escalón, techo y dt fijo; configuración expuesta al SDK.
2. Integrar cámara/jugador opt-in sin asumir armas; añadir waypoints/consultas mínimos para actores de escenas nuevas.
3. Crear recorrido de prueba con props, escalera y pasillo estrecho; registrar límites admitidos y comportamiento ante penetración inicial.

**Aceptación:**

- Movimiento estable con render a distintas frecuencias; no atraviesa suelo/techo/esquinas bajo velocidad máxima admitida.
- Jugador cabe/no cabe conforme a volumen, pendiente/escalón válidos y plataformas definidas; FPS no es dependencia del core.
- Navegación 3D verificable para el recorrido nuevo, sin afirmar cobertura navmesh que no existe.

Verificación: V-CORE, V-APP, V-SPATIAL.

Desbloquea: Exploración 3D y colisión de puertas.

Evidencia: docs/implementation/evidence/S02.md.

Commit integrado: `22e937a2d589e256f38ff7268603c26723343cae`.

## S03 — Puertas con bisagra, triggers e interacción

Fase: **P6** · Rol: **runtime** · Estado: **IN_PROGRESS**.

Dependencias: S02, D02, R03.

Locks: `gamekit`, `document-schema`.

Puntos de entrada: `engines/vestigio/src/gamekit/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gamekit/`, `engines/vestigio/tests/spatial/`.

**Trabajo:**

1. Implementar raíz/bisagra/panel con eje/pivot/ángulos/speed, estados closed/opening/open/closing/blocked/locked y obstrucción segura.
2. Mover collider con misma transform del panel, incluso totalmente abierto; sweep angular/substeps con límites derivados de dimensiones/velocidad.
3. Conectar key/lock/auto-close, Interactable y Trigger enter/exit/once/cooldown a eventos/reglas; emitir eventos de audio, reproducidos al integrar A01.

**Aceptación:**

- Puerta gira en ambos sentidos y con padre rotado; cierre sobre cuerpo bloquea/revierte sin aplastar por defecto ni tunneling dentro de límites probados.
- Raycast/visión/proyectil/movimiento coinciden con panel y collider abierto sigue sólido.
- Definición de puerta/triggers sobrevive round-trip; estado mutable serializable queda listo para P01. Sonido sólo se acepta como conectado después de A01/E05.
- Acciones, puerta y volumen trigger tienen IDs/eventos persistentes; entrar/salir y abrir/cerrar producen feedback visible y collider concordante en Play.

Verificación: V-CORE, V-DATA, V-SPATIAL.

Desbloquea: Interacción física de horror/exploración.

## V01 — Materiales, shaders editables, luces y fog GPU

Fase: **P7** · Rol: **gpu** · Estado: **IN_PROGRESS**.

Dependencias: G03, I01, D02.

Locks: `gpu-backend`, `shader-schema`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/assets/`, `engines/vestigio/src/content/`, `engines/vestigio/assets/shaders/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Implementar ShaderAsset/material params tipados y metadata de inspector; reload candidato con dependencias y log de compilación.
2. Añadir forward simple, luces por draw limitadas/priorizadas y fog lineal/exp con distancia definida, alpha y sky coherentes.
3. Registrar light/material/environment schemas para documento/SDK sin escribir UI paralela; budgets/overlays muestran límites excedidos.

**Aceptación:**

- Shader inválido conserva el anterior; primer fallo tiene error material/diagnóstico explícito y no mata sesión.
- Luces/fog se ejecutan en GPU y sus parámetros sobreviven save/load; no se atribuyen sombras geométricas inexistentes.
- Entradas/espacio de color/unidades y límites son probados; no se incorpora PBR/volumétricos como dependencia del hito.
- Luces y fog de distancia no sustituyen niebla por capas ni volumen con depth; los presets de niebla locales se validan por separado en V03/V04.

Verificación: V-APP, V-GPU, V-ASSET, V-DATA.

Desbloquea: Estilo visual editable sin bifurcar renderer.

## V02 — Perfiles Neo-PSX, upscale y settings de vídeo

Fase: **P7** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`, `input-settings`, `shader-schema`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/config/`, `engines/vestigio/assets/shaders/`, `engines/vestigio/tests/gpu/`, `engines/vestigio/tests/config/`.

**Trabajo:**

1. Implementar presets clean/retro/PSX/custom con snapping, UV affine opcional, Bayer/dither y cuantización después de fog/lighting a resolución interna.
2. Resolver ping-pong postprocess, HUD pixelado/nítido, filtro, escala entera/fraccional/aspecto, fullscreen/borderless y rollback de targets/settings.
3. Separar capacidades de backend de perfil; ningún perfil GPU fuerza readback ni exige paridad de custom shaders con renderer CPU.

**Aceptación:**

- Cambiar perfil en el mismo nivel funciona; snapping cerca del near plane no rompe clipping y affine no es un efecto de ruido de pantalla.
- 320x180/426x240/640x360 conservan aspecto en resize/DPI; barras y dither permanecen ligados a píxel interno.
- Settings persisten con overrides correctos, targets viejos se liberan y efectos pueden desactivarse sin editar assets.
- El orden de passes, perfil PSX y resize quedan medidos en GPU; V05 cubre la pila adicional de efectos.

Verificación: V-CORE, V-APP, V-GPU, V-DATA.

Desbloquea: Neo-PSX como perfil del mismo runtime.

## A01 — Audio de proyecto, voces, música y emisores

Fase: **P7** · Rol: **runtime** · Estado: **IN_PROGRESS**.

Dependencias: R02, I01, D02.

Locks: `audio-core`, `public-api`.

Puntos de entrada: `engines/vestigio/src/audio/`, `engines/vestigio/src/assets/`, `engines/vestigio/src/api/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/audio/`.

**Trabajo:**

1. Separar Sound compartido, Voice mutable y Music streaming; buses master/music/SFX/ambience y listener/emisor espacial.
2. Conectar eventos de interacción al playback sin repetir sonidos por frame; límites de voces y política de prioridad explícitos.
3. Exponer configuración al SDK y esquema documental, lifecycle de streaming y pausa/foco/cambio de mundo; registrar contratos reales de pan/volumen de backend.

**Aceptación:**

- Dos emisores comparten Sound pero tienen voces independientes; descargar mundo detiene sus voces sin invalidar otros usuarios.
- Música se actualiza sin cargar todo como PCM y buses persisten; dispositivo ausente/fallo se comunica sin crash.
- Escucha real confirma posición/volumen/evento y pausa; pruebas automatizadas cubren ownership y límites, no sustituyen audición.
- Emisores posicionales, pasos, reverberacion y musica adaptable se separan en A03-A06 con audicion real.

Verificación: V-CORE, V-APP, V-ASSET, V-AUDIO, V-DATA.

Desbloquea: Puertas audibles, ambiente y música de juego.

## A02 — Animación rígida y skeletal con pose por instancia

Fase: **P7** · Rol: **content** · Estado: **IN_PROGRESS**.

Dependencias: G03, D02.

Locks: `asset-import`, `gpu-skinning`, `gpu-backend`, `document-schema`.

Puntos de entrada: `engines/vestigio/src/assets/import/`, `engines/vestigio/src/world/`, `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/assets/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Extender importer/IR con clips de nodos y después skins/joints/inverse bind matrices; definir interpolaciones soportadas y rechazar el resto explícitamente.
2. Separar asset de animación/mesh compartido de tiempo/pose por instancia; skinning GPU dentro de capacidades/budgets definidos.
3. Serializar parámetros de instancia; coordinar layout de atributos/uniforms con responsable GPU antes de modificar backend.

**Aceptación:**

- Dos instancias del mismo asset animan con tiempos diferentes sin duplicar mesh ni pose compartida accidental.
- Pose de reposo/ejes/normales y jerarquía coinciden con fixtures numéricas/visuales; error de skin inválido no publica recurso parcial.
- GPU ejecuta skinning en perfil soportado, recursos se liberan y clip/velocidad/default persisten; morph targets no se anuncian si no se implementan.
- Se prueba clip glTF importado por instancia y skin/skeleton con pausa, crossfade, evento de fin y round-trip de referencia; el movimiento rigido hardcoded no lo satisface.

Verificación: V-CORE, V-ASSET, V-GPU, V-DATA.

Desbloquea: Personajes/modelos animados reales.

## UX01 — Studio utilizable para crear el primer nivel 3D propio

Fase: **P5-UX** · Rol: **editor** · Estado: **VERIFIED**.

Dependencias: E04.

Locks: `wpf-viewport`, `wpf-inspector`, `document-commands`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/studio.tests/`, `engines/vestigio/src/platform/`, `engines/vestigio/src/content/`, `engines/vestigio/src/player/`, `docs/implementation/evidence/UX01.md`.

**Trabajo:**

1. Ofrecer Nuevo nivel, Abrir, Guardar y Guardar como con estado sin guardar y Atrium como ejemplo optativo; un nivel nuevo no debe ser una copia del Atrium.
2. Reorganizar Studio en jerarquía, viewport GPU, inspector contextual y recursos/problemas; mostrar Construir habitación como herramienta y no como formulario permanente del inspector.
3. Cerrar un recorrido propio: crear habitación y abertura, importar/colocar un GLB, editar transformación, guardar, cerrar, abrir, Probar y Detener, usando las operaciones nativas existentes.

**Aceptación:**

- Una persona puede completar el recorrido en un nivel nuevo sin editar JSON ni usar comandos de terminal después de abrir Studio; el archivo reabierto conserva habitación, abertura, asset y transformación y se juega en el mismo runtime.
- Nuevo/Abrir/Cerrar respetan Cancelar ante cambios sin guardar; Probar/Detener no modifica el documento y vuelve a una selección y foco útiles.
- La composición real es legible y operable a 1366x768 y 1920x1080 con DPI 100% y 150%, sin controles esenciales recortados; captura y revisión interactiva del usuario son obligatorias antes de marcar UX01 INTEGRATED.
- Pruebas dirigidas cubren creación, apertura, guardado, cancelación, ida y vuelta, foco y Play/Stop; no se amplía a exportación, animación, diálogo, puertas complejas ni compatibilidad de proyectos anteriores.

Verificación: V-WPF, V-DATA, V-APP, V-GPU.

Desbloquea: Primer nivel 3D propio y editor aceptado visualmente antes de ampliar E05.

Evidencia: docs/implementation/evidence/UX01.md.

Origen: `Revisión del usuario de VESTIGIO Studio tras E04`, `docs/implementation/STUDIO-UX-RECOVERY.md`.

## E05 — Cerrar el recorrido editorial completo

Fase: **P5-P7** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E04, S03, V02, A01, A02.

Locks: `wpf-inspector`, `wpf-viewport`, `tool-api`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/editor/`, `engines/vestigio/studio.tests/`, `engines/vestigio/tests/journeys/`.

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

Dependencias: E05.

Locks: `profiling`, `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/debug/`, `engines/vestigio/src/render/`, `engines/vestigio/src/runtime/`, `engines/vestigio/tests/perf/`, `docs/implementation/evidence/`.

**Trabajo:**

1. Consolidar consola/overlays, CPU simulation/prepare/submit/present, GPU por pass cuando soportado, p50/p95/p99, draw calls, uploads/readbacks, entidades/assets y RAM/VRAM estimada.
2. Medir corpus integrado en hardware registrado, con warmup/duración/configuración reproducible; separar integrado/dedicado si se dispone de ambos.
3. Optimizar sólo cuellos medidos y revalidar; fijar límites operativos y degradación explícita de luces/efectos, sin inventar 60 FPS garantizados.

**Aceptación:**

- Reportes distinguen estimación de memoria de medición driver y coste CPU de GPU; queries de tiempo no bloquean cada frame.
- Recargar mundo/shader/assets repetidamente no presenta crecimiento sostenido de recursos propios; capturas no contaminan cifras del frame normal.
- Presupuesto aceptado y limitaciones quedan documentados para hardware realmente probado; ausencia de otro equipo queda pendiente explícita.
- Medir costes de niebla, postprocesado, particulas, audio, instancing y escena guiada en hardware documentado.

Verificación: V-APP, V-GPU, V-PERF.

Desbloquea: Criterio de rendimiento de producto con evidencia.

## P01 — Savegame versionado por componente

Fase: **P8** · Rol: **content** · Estado: **PLANNED**.

Dependencias: S03, A01, A02, R03, D02.

Locks: `savegame`, `document-core`, `gamekit`.

Puntos de entrada: `engines/vestigio/src/content/`, `engines/vestigio/src/gamekit/`, `engines/vestigio/tests/content/`, `engines/vestigio/tests/gamekit/`.

**Trabajo:**

1. Separar nivel/autosave/partida y definir persistencia de jugador, puertas, actores, variables, inventario, RNG, triggers y estado de módulo C.
2. Resolver transitorios: proyectiles/timers/animación/música se persisten o reinician según contrato explícito; nunca snapshot de punteros/memoria arbitraria.
3. Restaurar candidato contra ProjectId/LevelId/content revision con migración o error; guardar de forma recuperable.

**Aceptación:**

- Guardar a mitad de apertura y restaurar conserva ángulo/lock/timer admitidos; actores/variables y transitorios siguen política declarada.
- Save corrupto/incompatible no aplica estado a otras entidades ni destruye partida anterior.
- Juego C puede aportar estado versionado sin modificar core; lifecycle de recursos/voces tras restore queda verificado.
- Estado de escenas, interacciones, puzzles, secuencias, inventario y vitals usa esquema versionado por componente y lectura posterior completa.

Verificación: V-CORE, V-APP, V-DATA, V-SPATIAL, V-AUDIO.

Desbloquea: Persistencia de partida y entrega independiente.

## P02 — Dos juegos y exportación independiente del repositorio

Fase: **P8** · Rol: **integrator** · Estado: **PLANNED**.

Dependencias: E05, P01, Q01, J01.

Locks: `integration`, `build`, `public-api`, `packaging`.

Puntos de entrada: `engines/vestigio/examples/`, `engines/vestigio/cmake/`, `tools/export-project.ps1`, `engines/vestigio/CMakeLists.txt`, `THIRD_PARTY.md`, `engines/vestigio/tests/sdk/`, `engines/vestigio/tests/export/`.

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

Dependencias: P02.

Locks: `tool-api`, `cli`.

Puntos de entrada: `engines/vestigio/src/cli/`, `engines/vestigio/src/editor/`, `engines/vestigio/tests/cli/`, `docs/`.

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

Dependencias: P02, T01.

Locks: `integration`, `build`, `packaging`.

Puntos de entrada: `docs/implementation/STATE.md`, `docs/implementation/evidence/`, `docs/`, `engines/vestigio/tests/`.

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

## H01 — Colocacion directa en viewport Studio

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E03.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Crear, seleccionar y mover un objeto en la escena viva sin modificar Play al volver a Editar.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Crear, seleccionar y mover un objeto en la escena viva sin modificar Play al volver a Editar.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/SceneEditor.js`.

## H02 — Catalogo GLB y seleccion de submodelos

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E03, M01.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Elegir un GLB y un nodo o submodelo identificable y conservar su referencia al reabrir.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Elegir un GLB y un nodo o submodelo identificable y conservar su referencia al reabrir.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/SceneEditor.js`, `assets/models/manifest.json`.

## H03 — Props procedurales editables

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E03.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Crear un prop parametrico desde Studio y regenerarlo sin perder materiales ni colision.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Crear un prop parametrico desde Studio y regenerarlo sin perder materiales ni colision.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/PropFactory.js`.

## H04 — Colocacion visual de luces y patrones

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E03, V01.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Colocar point spot y directional con color intensidad alcance sombra y parpadeo reproducible.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Colocar point spot y directional con color intensidad alcance sombra y parpadeo reproducible.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/LightPlacer.js`, `engine/effects/FlickerLight.js`.

## H05 — Ventanas marcos y vidrio en huecos

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E04, S03.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Insertar una ventana editable en un muro y mantener vano marco vidrio y collider alineados.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Insertar una ventana editable en un muro y mantener vano marco vidrio y collider alineados.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/WallHoleTool.js`.

## H06 — Plantas y conectores entre pisos

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E04, S02.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Crear pisos y escalera o escalera vertical con navegacion y luces asignadas por piso.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Crear pisos y escalera o escalera vertical con navegacion y luces asignadas por piso.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/FloorManager.js`.

## H07 — Zonas trigger dibujadas en Studio

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E04, S03.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Dibujar volumen enter exit once cooldown y evento con vista previa en Play.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Dibujar volumen enter exit once cooldown y evento con vista previa en Play.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/TriggerZonePainter.js`, `engine/TriggerZone.js`.

## H08 — Gestion de escenas y transicion segura

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E05.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Cargar escena candidata y cambiar solo tras validarla conservando la escena previa ante error.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Cargar escena candidata y cambiar solo tras validarla conservando la escena previa ante error.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/SceneManager.js`, `engine/SceneLoader.js`.

## H09 — Recetas de salas poligonales

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: E04.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Editar perimetro irregular con suelo techo paredes UV y colliders derivados del mismo poligono.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Editar perimetro irregular con suelo techo paredes UV y colliders derivados del mismo poligono.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/RoomBuilder.js`.

## H10 — Huecos verticales entre pisos

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: H06, H09.

Locks: `document-core`, `wpf-viewport`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Editar hueco de piso persistente con geometria collider y paso vertical coherentes.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Editar hueco de piso persistente con geometria collider y paso vertical coherentes.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-DATA, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `.planning/requirements/v6.0-REQUIREMENTS.md`.

## V03 — Niebla local por capas y ruido animado

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Reproducir capas filamentos viento y densidad espacial con parametros de escena persistentes.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Reproducir capas filamentos viento y densidad espacial con parametros de escena persistentes.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/ExteriorFog.js`, `engine/effects/FogShaders.js`.

## V04 — Niebla volumetrica con profundidad real

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Muestrear depth valido y componer raymarch antes del perfil retro con oclusion verificable.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Muestrear depth valido y componer raymarch antes del perfil retro con oclusion verificable.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/VolumetricFog.js`.

## V05 — Pila de postprocesado configurable

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V02.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Configurar passes y orden incluido PSX bloom glitch y aberracion sin degradar resize.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Configurar passes y orden incluido PSX bloom glitch y aberracion sin degradar resize.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/PostProcessor.js`, `engine/shaders/HorrorFXShader.js`.

## V06 — Materiales especiales editables

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Asignar presets fluid neon organico charco TV y holograma con parametros por material.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Asignar presets fluid neon organico charco TV y holograma con parametros por material.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/shaders/FluidShader.js`, `engine/shaders/NeonShader.js`, `engine/shaders/OrganicShader.js`.

## V07 — Emisor de particulas reutilizable

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Emitir y reciclar particulas con limite medible y estado claro al cambiar escena.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Emitir y reciclar particulas con limite medible y estado claro al cambiar escena.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/ParticleSystem.js`.

## V08 — Fuego con humo y luz

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V07, V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Instanciar fuego con emisor humo y luz vinculada y liberar recursos al retirar entidad.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Instanciar fuego con emisor humo y luz vinculada y liberar recursos al retirar entidad.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/FireSystem.js`.

## V09 — Decals y vidrio rompible

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V07, S03.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Proyectar decals limitados y romper vidrio sincronizando visual colision y evento.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Proyectar decals limitados y romper vidrio sincronizando visual colision y evento.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/DecalSystem.js`, `engine/effects/BreakableGlass.js`.

## V10 — Agua y estado submarino

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V06, S02.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Crear superficie animada y entrada salida del agua con render audio y movimiento coherentes.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Crear superficie animada y entrada salida del agua con render audio y movimiento coherentes.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/WaterSystem.js`.

## V11 — Cielo y ciclo de iluminacion

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Configurar cielo sol estrellas y hora persistente con lectura visible en Play.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Configurar cielo sol estrellas y hora persistente con lectura visible en Play.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/SkySystem.js`.

## V12 — Clima y precipitacion

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V07, V11.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Activar lluvia truenos viento y niebla ambiental con limites y configuracion de nivel.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Activar lluvia truenos viento y niebla ambiental con limites y configuracion de nivel.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/Weather.js`.

## V13 — Vegetacion instanciada

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V07.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Pintar vegetacion con viento y contador de instancias y liberar lote al descargar.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Pintar vegetacion con viento y contador de instancias y liberar lote al descargar.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/VegetationSystem.js`.

## V14 — Espejos de escena

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Reflejar escena con camara y target controlados tras resize y descarga.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Reflejar escena con camara y target controlados tras resize y descarga.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/MirrorSystem.js`.

## V15 — Destellos de lente con oclusion

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Mostrar destello solo con fuente visible y presupuesto de coste medido.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Mostrar destello solo con fuente visible y presupuesto de coste medido.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/LensFlare.js`.

## V16 — Video sobre superficies

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V01.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Reproducir pausar y liberar video texturizado en una entidad de nivel.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Reproducir pausar y liberar video texturizado en una entidad de nivel.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/VideoPlayer.js`.

## V17 — Superficies organicas y efecto corporal

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V06, V07.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/src/content/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Instanciar capa organica compartiendo material y sincronizar shader y estado de juego.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Instanciar capa organica compartiendo material y sincronizar shader y estado de juego.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-GPU, V-APP, V-PERF.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/effects/FleshInfestation.js`.

## K01 — Acciones contextuales con condiciones

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: S03.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Mostrar acciones disponibles bloqueo y respuesta visible desde distancia y foco correctos.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Mostrar acciones disponibles bloqueo y respuesta visible desde distancia y foco correctos.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/InteractionSystem.js`, `engine/ui/ContextMenu.js`.

## K02 — Eventos y maquina de estados de juego

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K01.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Definir eventos tipados y transiciones deterministas sin perder estado al guardar.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Definir eventos tipados y transiciones deterministas sin perder estado al guardar.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/EventBus.js`, `engine/StateMachine.js`.

## K03 — Secuencias narrativas de juego

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K02.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Ejecutar dialogo espera flag audio y eleccion con pausa cancelacion y restauracion definidas.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Ejecutar dialogo espera flag audio y eleccion con pausa cancelacion y restauracion definidas.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ScriptEngine.js`, `game/apartment/Day1Script.js`.

## K04 — Timeline y camara cinematica

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K03, A02.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Editar keyframes y curva de camara con eventos sincronizados y salida limpia a control del jugador.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Editar keyframes y curva de camara con eventos sincronizados y salida limpia a control del jugador.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/Timeline.js`, `engine/CameraDolly.js`.

## K05 — Puzzles reutilizables

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K01, K02.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Resolver combinacion terminal cables o sliders mediante estado y eventos guardables.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Resolver combinacion terminal cables o sliders mediante estado y eventos guardables.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/PuzzleManager.js`, `engine/ui/PuzzleHelpers.js`.

## K06 — Transporte de objetos

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K05, S02.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Recoger sostener soltar y depositar objeto con colision foco y estado reproducible.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Recoger sostener soltar y depositar objeto con colision foco y estado reproducible.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/CarrySystem.js`.

## K07 — IA basica de NPC

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K02, S02.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Ejecutar idle investigate chase lost con percepcion y transiciones visibles.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Ejecutar idle investigate chase lost con percepcion y transiciones visibles.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/AIController.js`.

## K08 — Estado y movimiento del jugador

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: S02.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Exponer salud oxigeno hambre cordura stamina y estados de movimiento con guardado.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Exponer salud oxigeno hambre cordura stamina y estados de movimiento con guardado.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/FPSController.js`, `engine/PlayerVitals.js`.

## K09 — Linterna con bateria

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K08, V01.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Iluminar desde jugador con bateria parpadeo y cono consistente con fog.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Iluminar desde jugador con bateria parpadeo y cono consistente con fog.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/Flashlight.js`.

## K10 — Armas e impactos

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K07, K08.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Equipar disparar recargar y emitir impacto con municion y estado guardables.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Equipar disparar recargar y emitir impacto con municion y estado guardables.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/Weapon.js`.

## K11 — Ragdoll acotado

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K10, A02.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Activar cuerpo fisico temporal con limite y limpieza sin fugas.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Activar cuerpo fisico temporal con limite y limpieza sin fugas.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-CORE, V-APP, V-DATA.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/Ragdoll.js`.

## A03 — Audio posicional 3D

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: A01.

Locks: `audio-core`.

Puntos de entrada: `engines/vestigio/src/audio/`, `engines/vestigio/tests/audio/`.

**Trabajo:**

1. Oir emisor con distancia y orientacion correctas en Player y escena.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Oir emisor con distancia y orientacion correctas en Player y escena.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-AUDIO, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/AudioWorld.js`.

## A04 — Zonas de reverberacion

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: A03.

Locks: `audio-core`.

Puntos de entrada: `engines/vestigio/src/audio/`, `engines/vestigio/tests/audio/`.

**Trabajo:**

1. Cruzar zona y cambiar envio de reverb sin recrear voces ni acumular nodos.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Cruzar zona y cambiar envio de reverb sin recrear voces ni acumular nodos.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-AUDIO, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ReverbZones.js`.

## A05 — Pasos por superficie

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: A03, S02.

Locks: `audio-core`.

Puntos de entrada: `engines/vestigio/src/audio/`, `engines/vestigio/tests/audio/`.

**Trabajo:**

1. Escuchar pasos segun material velocidad y estado de movimiento.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Escuchar pasos segun material velocidad y estado de movimiento.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-AUDIO, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/FootstepSystem.js`.

## A06 — Musica adaptable

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: A01, K02.

Locks: `audio-core`.

Puntos de entrada: `engines/vestigio/src/audio/`, `engines/vestigio/tests/audio/`.

**Trabajo:**

1. Cambiar stems o capas por tension con crossfade y pausa estable.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Cambiar stems o capas por tension con crossfade y pausa estable.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-AUDIO, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/MusicDirector.js`.

## U01 — Dialogo y subtitulos

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: K03.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Mostrar dialogo y subtitulos sincronizados con avance pausa y accesibilidad.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Mostrar dialogo y subtitulos sincronizados con avance pausa y accesibilidad.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ui/DialogManager.js`, `engine/ui/SubtitleUI.js`.

## U02 — Elecciones y menu contextual

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: K01, K03.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Elegir accion narrativa y devolver control sin conflicto de pointer lock.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Elegir accion narrativa y devolver control sin conflicto de pointer lock.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ui/ChoiceUI.js`, `engine/ui/ContextMenu.js`.

## U03 — Inventario de objetos

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: K05.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Recoger usar y quitar item con estado guardado y feedback.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Recoger usar y quitar item con estado guardado y feedback.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ui/InventoryUI.js`.

## U04 — Inspeccion y revelacion de items

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: U03, H02.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Examinar objeto 3D y volver al juego liberando recursos y foco.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Examinar objeto 3D y volver al juego liberando recursos y foco.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ui/ItemInspectUI.js`, `engine/ui/ItemRevealUI.js`.

## U05 — Documentos y codex

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: U03.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Abrir nota y registro persistente con texto subtitulado.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Abrir nota y registro persistente con texto subtitulado.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- Crear -> guardar -> cerrar -> abrir -> jugar conserva parametros, referencias y comportamiento; undo/redo se prueba si hay autoria.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ui/DocumentUI.js`, `engine/ui/CodexUI.js`.

## U06 — HUD y HUD diegetico

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: K08.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Mostrar estado con modo plano o diegetico y actualizacion sin duplicados.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Mostrar estado con modo plano o diegetico y actualizacion sin duplicados.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ui/HUD.js`, `engine/ui/DiegeticHUD.js`.

## U07 — Telefono interactivo

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: K03.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Recibir y responder evento telefonico dentro de secuencia guardable.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Recibir y responder evento telefonico dentro de secuencia guardable.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ui/PhoneUI.js`.

## U08 — Temas e idiomas

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: U01.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Cambiar tema e idioma en runtime con cadenas externas y fallback.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Cambiar tema e idioma en runtime con cadenas externas y fallback.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/Theme.js`, `engine/i18n.js`.

## U09 — Ajustes foco y pantallas

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: I01, U08.

Locks: `ui-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/runtime/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Persistir ajustes y transiciones sin acciones pegadas ni pointer lock perdido.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Persistir ajustes y transiciones sin acciones pegadas ni pointer lock perdido.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-WPF, V-APP.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `engine/ui/SettingsMenu.js`, `engine/UIFocusManager.js`, `engine/ui/ScreenManager.js`.

## X02 — Procedencia y permisos de assets candidatos

Fase: **P12** · Rol: **content** · Estado: **PLANNED**.

Dependencias: H02.

Locks: `asset-import`.

Puntos de entrada: `engines/vestigio/src/assets/`, `docs/research/`, `THIRD_PARTY.md`.

**Trabajo:**

1. Registrar fuente autor licencia y uso permitido de cada GLB antes de incorporarlo.
2. Definir componente y contrato del documento/runtime/Studio antes de cablear la escena de prueba; evitar copiar el acoplamiento Three.js/DOM.

**Aceptación:**

- Registrar fuente autor licencia y uso permitido de cada GLB antes de incorporarlo.
- La prueba dirigida registra comportamiento real en Player y/o Studio, errores y limpieza de recursos; codigo presente por si solo no cuenta.
- La funcion se activa por proyecto mediante contrato del nucleo y no impone su uso a otros juegos.

Verificación: V-ASSET, V-DELIVERY.

Desbloquea: Capacidad heredada verificable e independiente.

Origen: `assets/models/README.md`, `assets/models/manifest.json`.

## H11 — Elevador y transicion entre pisos

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: H06, H08, S03.

Locks: `document-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Crear cabina y puertas con destino de piso, guardar y reabrir; Player cambia piso sin perder estado.
2. Definir contrato de datos, componente del nucleo y activacion por proyecto; integrar Studio y Player cuando aplique.

**Aceptación:**

- Crear cabina y puertas con destino de piso, guardar y reabrir; Player cambia piso sin perder estado.
- Prueba dirigida de uso y cierre de recursos; guardar -> cerrar -> abrir -> jugar si hay estado autorable o de partida.

Verificación: V-WPF, V-APP, V-DATA.

Desbloquea: Idea heredada con entrega independiente.

Origen: `.planning/research/FEATURES.md`.

## H12 — Prefab interactivo de pod y contenedor

Fase: **P9** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: H02, H05, K01.

Locks: `document-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Instanciar pod configurable con vidrio, apertura y eventos sin geometria fija de juego.
2. Definir contrato de datos, componente del nucleo y activacion por proyecto; integrar Studio y Player cuando aplique.

**Aceptación:**

- Instanciar pod configurable con vidrio, apertura y eventos sin geometria fija de juego.
- Prueba dirigida de uso y cierre de recursos; guardar -> cerrar -> abrir -> jugar si hay estado autorable o de partida.

Verificación: V-WPF, V-APP, V-DATA.

Desbloquea: Idea heredada con entrega independiente.

Origen: `.planning/requirements/v6.0-REQUIREMENTS.md`.

## K12 — Presets de criaturas sobre IA y animacion

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K07, A02.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/src/input/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Crear criatura con percepcion, animacion y perfil de conducta configurable por proyecto.
2. Definir contrato de datos, componente del nucleo y activacion por proyecto; integrar Studio y Player cuando aplique.

**Aceptación:**

- Crear criatura con percepcion, animacion y perfil de conducta configurable por proyecto.
- Prueba dirigida de uso y cierre de recursos; guardar -> cerrar -> abrir -> jugar si hay estado autorable o de partida.

Verificación: V-CORE, V-APP.

Desbloquea: Idea heredada con entrega independiente.

Origen: `.planning/research/FEATURES.md`.

## K13 — Golpes por parte y desmembramiento

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: K10, K12, V09.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/src/input/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Impactos por hitbox cambian malla y estado de IA con evidencia visual y guardado.
2. Definir contrato de datos, componente del nucleo y activacion por proyecto; integrar Studio y Player cuando aplique.

**Aceptación:**

- Impactos por hitbox cambian malla y estado de IA con evidencia visual y guardado.
- Prueba dirigida de uso y cierre de recursos; guardar -> cerrar -> abrir -> jugar si hay estado autorable o de partida.

Verificación: V-CORE, V-APP.

Desbloquea: Idea heredada con entrega independiente.

Origen: `.planning/research/FEATURES.md`.

## U10 — Inventario espacial de cuadricula

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: U03.

Locks: `document-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Mover y rotar items con reglas de espacio, apilado y persistencia reproducible.
2. Definir contrato de datos, componente del nucleo y activacion por proyecto; integrar Studio y Player cuando aplique.

**Aceptación:**

- Mover y rotar items con reglas de espacio, apilado y persistencia reproducible.
- Prueba dirigida de uso y cierre de recursos; guardar -> cerrar -> abrir -> jugar si hay estado autorable o de partida.

Verificación: V-WPF, V-APP, V-DATA.

Desbloquea: Idea heredada con entrega independiente.

Origen: `.planning/research/FEATURES.md`.

## U11 — Inventario holografico diegetico

Fase: **P11** · Rol: **editor** · Estado: **PLANNED**.

Dependencias: U03, U06, V06.

Locks: `document-core`.

Puntos de entrada: `engines/vestigio/studio/`, `engines/vestigio/src/content/`, `engines/vestigio/studio.tests/`.

**Trabajo:**

1. Abrir inventario proyectado con seleccion y foco correctos sin segunda escena permanente.
2. Definir contrato de datos, componente del nucleo y activacion por proyecto; integrar Studio y Player cuando aplique.

**Aceptación:**

- Abrir inventario proyectado con seleccion y foco correctos sin segunda escena permanente.
- Prueba dirigida de uso y cierre de recursos; guardar -> cerrar -> abrir -> jugar si hay estado autorable o de partida.

Verificación: V-WPF, V-APP, V-DATA.

Desbloquea: Idea heredada con entrega independiente.

Origen: `.planning/research/FEATURES.md`.

## X03 — Modo foto y captura reproducible

Fase: **P10** · Rol: **gpu** · Estado: **PLANNED**.

Dependencias: V05, K04.

Locks: `gpu-backend`.

Puntos de entrada: `engines/vestigio/src/render/`, `engines/vestigio/tests/gpu/`.

**Trabajo:**

1. Pausar juego, mover camara libre y exportar captura sin alterar estado ni recursos residentes.
2. Definir contrato de datos, componente del nucleo y activacion por proyecto; integrar Studio y Player cuando aplique.

**Aceptación:**

- Pausar juego, mover camara libre y exportar captura sin alterar estado ni recursos residentes.
- Prueba dirigida de uso y cierre de recursos; guardar -> cerrar -> abrir -> jugar si hay estado autorable o de partida.

Verificación: V-GPU, V-APP.

Desbloquea: Idea heredada con entrega independiente.

Origen: `.planning/research/FEATURES.md`.

## I02 — Control tactil y viewport movil

Fase: **P11** · Rol: **runtime** · Estado: **PLANNED**.

Dependencias: I01, U09.

Locks: `runtime-core`.

Puntos de entrada: `engines/vestigio/src/runtime/`, `engines/vestigio/src/input/`, `engines/vestigio/tests/runtime/`.

**Trabajo:**

1. Usar acciones equivalentes en interfaz tactil y validar layout, foco y rendimiento en dispositivo objetivo.
2. Definir contrato de datos, componente del nucleo y activacion por proyecto; integrar Studio y Player cuando aplique.

**Aceptación:**

- Usar acciones equivalentes en interfaz tactil y validar layout, foco y rendimiento en dispositivo objetivo.
- Prueba dirigida de uso y cierre de recursos; guardar -> cerrar -> abrir -> jugar si hay estado autorable o de partida.

Verificación: V-CORE, V-APP.

Desbloquea: Idea heredada con entrega independiente.

Origen: `engine/TouchInput.js`, `.planning/ROADMAP.md`.

## X01 — Demo guiada y QA visual de las capacidades heredadas

Fase: **P12** · Rol: **integrator** · Estado: **PLANNED**.

Dependencias: H01, H02, H03, H04, H05, H06, H07, H08, H09, H10, V03, V04, V05, V06, V07, V08, V09, V10, V11, V12, V13, V14, V15, V16, V17, K01, K02, K03, K04, K05, K06, K07, K08, K09, K10, K11, A03, A04, A05, A06, U01, U02, U03, U04, U05, U06, U07, U08, U09, H11, H12, K12, K13, U10, U11, X03, I02.

Locks: `integration`, `build`, `packaging`.

Puntos de entrada: `engines/vestigio/examples/`, `engines/vestigio/tests/`, `docs/implementation/evidence/`.

**Trabajo:**

1. Construir recorrido guiado con estaciones que ejerzan cada capacidad heredada y registren proyecto, hardware, captura y consola.
2. Probar crear -> guardar -> cerrar -> abrir -> jugar en tipos autorables y recorridos de juego en Player/Studio.

**Aceptación:**

- Cada estacion tiene prueba reproducible y resultado PASS, FAIL o NOT_RUN; captura y consola se revisan en navegador real.
- Ninguna ausencia de navegador o aceptacion humana se transforma en PASS.

Verificación: V-APP, V-GPU, V-WPF, V-DELIVERY.

Desbloquea: Candidato expandido observable.

Origen: `game/sandbox/main.js`, `game/sandbox/SandboxLevel.js`, `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-QA-CHECKLIST.md`.

## ZA1 — Aceptacion integrada de la ampliacion js-game

Fase: **P12** · Rol: **integrator** · Estado: **PLANNED**.

Dependencias: Z01, UX01, X01, X02.

Locks: `integration`, `build`, `packaging`.

Puntos de entrada: `docs/implementation/`, `docs/research/`, `engines/vestigio/tests/`.

**Trabajo:**

1. Integrar evidencia de Z01, demo X01 y procedencia X02 en un unico candidato.
2. Auditar matriz de capacidades y tickets contra implementacion, limitaciones y aprobacion visual humana.

**Aceptación:**

- Todas las capacidades marcadas para traslado tienen evidencia en el candidato o estado incompleto explicito.
- Plan, pruebas y aprobacion humana se reportan por separado; cero PASS inferidos de documentos historicos.

Verificación: V-CORE, V-APP, V-GPU, V-WPF, V-ASSET, V-DATA, V-AUDIO, V-PERF, V-DELIVERY.

Desbloquea: Cierre verificable del alcance ampliado.

Origen: `docs/research/13-js-game-full-audit.md`.
