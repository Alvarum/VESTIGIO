# 03 — Investigación selectiva de referencias

Consulta: 2026-09-20. Repositorios fijados por commit en [procedencia](05-source-provenance.md). [Índice de fuentes](sources-index.md) contiene enlaces por archivo, revisión y SHA-256. Se descargaron fuentes seleccionadas a un directorio temporal para lectura; no se incorporó código externo al engine. No se ejecutaron estos motores ni sus editores.

Categorías: **A** idea arquitectónica, **B** algoritmo, **C** patrón de UX, **D** representación de datos, **E** candidato a reutilización directa. Una inferencia sobre productividad se identifica como tal: no se confunde código de herramientas con evaluación humana.

## FPS Creator Classic — reducir la distancia entre colocar y jugar

Pregunta: ¿cómo hacer que una puerta/enemigo sea contenido utilizable sin programar todo su comportamiento?

Base de rutas: `Dark Basic Pro Shared/Dark Basic Pro/Projects/FPSCREATOR/`. Véanse archivos enlazados en [fuentes de FPS Creator](sources-index.md#fps-creator-classic).

| Archivo / símbolo o sección | Hallazgo | Categoría / decisión |
|---|---|---|
| `Files/entitybank/ww2/scenery/doors/door_a_k.fpe`, `aiinit`, `aimain`, `aidestroy`, `usekey` | Una definición vincula modelo, textura, scripts, llave, sonidos y animación | D/C: presets de entidad completos con defaults editables |
| `Files/scriptbank/dooruse.fpi`, bloques `state`, `activated`, `plrusingaction`, `sound`, `coloff/colon` | Interacción por proximidad dispara estados y animación; la colisión se activa/desactiva explícitamente | A: transiciones/eventos; no portar sintaxis ni apagado total de collider |
| `FPSC-MapEditor/FPSC-MapFile.dba`, `_mapfile_saveproject_fpm`, `_mapfile_loadproject_fpm` | Proyecto empaqueta datos de trabajo; guardar/cargar tiene ruta propia | D: paquete de juego derivado del documento |
| `FPSC-MapEditor/FPSC-MapEditorMain.dba`, llamada `_version_buildgame` | El workflow de construcción forma parte del editor | C: Probar/Exportar visibles; implementación completa de buildgame no auditada |
| `README.md`, instrucciones de compilación | Shell/editor de mapas/juego se construyen como ejecutables diferentes | A: piezas separadas; no copiar la división tecnológica histórica |

Inferencia de UX: biblioteca con objetos que ya saben interactuar + edición de propiedades + prueba del nivel reduce trabajo repetitivo. VESTIGIO ya tiene reglas y asistentes `re_editor_add_drop_rule`; conviene extender esa vía con presets, referencias por selector y validación.

No aprender de FPSC una API C moderna: el código seleccionado usa estado y `gosub` propios de DarkBASIC. La licencia del README no es una licencia open source y limita el uso del código. Decisión: conceptos/UX solamente, ninguna reutilización directa.

## DOOM — simulación simple y contenido con estados

Pregunta: ¿qué conviene conservar de un FPS clásico sin convertir el mundo en un formato Doom?

Fuentes: [DOOM](sources-index.md#doom), carpeta `linuxdoom-1.10/`.

| Archivo / símbolo | Hallazgo | Aplicación |
|---|---|---|
| `d_main.c`, `D_DoomLoop`; `p_tick.c`, `P_Ticker` | Tick y presentación tienen responsabilidades distinguibles; pausa altera simulación | A: mantener el reloj fijo existente de VESTIGIO, no adoptar sus constantes históricas |
| `p_tick.c`, `P_AddThinker`, `P_RemoveThinker`, `P_RunThinkers` | Objetos activos reciben actualización; eliminación diferida | A: cola de destrucción al final del tick; no copiar sentinel de function pointer ni lista global |
| `p_mobj.c`, `P_SetMobjState`; `info.h`, `state_t` | Estado enlaza duración, próximo estado y acción | D/A: actor/arma data-driven, transiciones explicitadas y límites ante ciclos |
| `p_pspr.c`, `P_MovePsprites` | Armas de primera persona tienen estados de presentación/acción propios | A: separar arma/HUD del renderer y del actor genérico |
| `p_doors.c`, `EV_DoDoor`; `p_spec.c`, `P_CrossSpecialLine` | Specials/activación crean comportamiento temporal sobre sectores | A: triggers y acciones; puerta moderna como componente, no número mágico de línea |
| `r_defs.h`, `sector_t`, `line_t`; `p_map.c`, `P_CheckPosition` | Representación de mapa sirve a render y gameplay, con fuerte especialización espacial | D/B: única geometría autorizada y canales de consulta; no adoptar WAD/sectores como límite universal |

La separación gameplay/render no es aislamiento perfecto: comparten estructuras y globals. VESTIGIO ya tiene matemática propia, tests sin ventana y paso fijo; conservar esos contratos es más útil que transplantar arquitectura histórica. No se investigó todo el renderer ni la IA de enemigos en detalle; la recomendación usa el sistema de estados, no afirma equivalencia entre sus comportamientos y los actuales.

## Quake — engine/game y consultas 3D

Pregunta: ¿cómo puede el motor instanciar contenido cuyo comportamiento vive fuera del renderer?

Fuentes: [Quake](sources-index.md#quake), `WinQuake/`.

- `pr_edict.c`, `ED_LoadFromFile`: lee entidades, obtiene `classname`, busca función con `ED_FindFunction` y ejecuta el spawn mediante `PR_ExecuteProgram`. **A/D:** registro nombre→fábrica/comportamiento; en VESTIGIO IDs versionados y callbacks C. No necesitamos QuakeC para lograr esa separación.
- `pr_exec.c`, `PR_ExecuteProgram`; `progs.h`, `edict_t`: frontera de programa de juego con representación propia. **A:** module API pequeña y versionada. Evitar offsets de VM y punteros internos como API pública.
- `host.c`, `_Host_Frame`, `Host_ServerFrame`: separa comandos, red, servidor, cliente, pantalla y sonido. **A:** agenda de frame explícita. Networking inspira intención→simulación→snapshot; no obliga a implementar multiplayer.
- `cmd.c`, `Cbuf_Execute`, `Cmd_AddCommand`; `cvar.c`, `Cvar_RegisterVariable`: comandos distintos de variables. **A/C:** consola con registro tipado, ayuda, flags de persistencia y restricciones de mutación.
- `model.c`, `Mod_ForName`, `Mod_LoadBrushModel`: modelos cargados/caché y mundo compilado especializado. **A/D:** recursos compartidos y datos derivados; no hacer obligatorio un compilador BSP.
- `world.c`, `SV_Move`, `SV_ClipMoveToEntity`, `SV_RecursiveHullCheck`: consultar volumen barrido contra mundo/entidades. **B:** contrato de `sweep` que reporta fracción/normal/impacto; rediseñar implementación para mallas/BVH y puertas cinemáticas.
- `sv_main.c`, `SV_WriteEntitiesToClient`: serializa representación visible al cliente. **A:** separar identidad/estado de simulación de datos de presentación. El protocolo no se adopta.

Resultado: librería estática + callbacks antes que VM o game DLL. Console/CVars útiles desde temprano; networking aplazado. `host_speeds` y `serverprofile` en `host.c` justifican medir simulación, render y audio por separado en lugar de un único FPS.

## GZDoom — extensiones declarativas sin hipotecar el núcleo

Pregunta: ¿cómo expresar actores y materiales extensibles preservando compatibilidad?

Fuentes: [GZDoom](sources-index.md#gzdoom).

- `src/playsim/actor.h`, `AActor`, `Tick`, `SetState`: actores sobre thinkers con muchos campos de compatibilidad. `wadsrc/static/zscript/actors/actor.zs` aporta la cara de scripting. **A/D:** contenido con defaults/estados; no reproducir el tamaño de `AActor` ni compatibilidad con todos los juegos originales.
- `src/r_data/gldefs.cpp`, `GLDefsParser::ParseShader`, `ParsePointLight`: definiciones de shaders/luces desde contenido. **D:** material/shader declarativo con parámetros tipados y validación. Un archivo editable debe describir shader y valores, no una secuencia de llamadas OpenGL.
- `src/common/rendering/gl/gl_shader.cpp`, `FShader::Load`, `FShaderCollection::Compile`: compilar variantes, conservar programas y separar selección de ejecución. **A:** caché de shaders por fuente/defines/backend; última versión válida al fallar reload. La política de reload es propuesta, no afirmación sobre este archivo.
- `src/common/console/c_cvars.h`, `FBaseCVar`, `CVAR_ARCHIVE`, `CVAR_NOSET`, `CVAR_LATCH`: metadatos de settings controlan persistencia y aplicación. **A/D:** settings tipados con alcance y restart requirement.

La compatibilidad es una política costosa, no una colección gratis de flags. VESTIGIO debería garantizar sus versiones de proyecto/mapa y migraciones, sin prometer reproducción exacta de WAD/DECORATE/ZScript. GZDoom tiene backend OpenGL/Vulkan según su README; no se auditó Vulkan ni se recomienda empezar con dos APIs gráficas.

## GZDoom Builder — modos de edición y reversibilidad

Pregunta: ¿cómo mantener productiva la edición de geometría y propiedades?

Fuentes: [GZDoom Builder](sources-index.md#gzdoom-builder).

- `Source/Core/Editing/EditMode.cs`, `EditMode`: interfaz de modos/herramientas, no una condición gigante en cada evento del viewport. **A/C:** activar/desactivar, preview, aceptar/cancelar, selección y navegación con contratos.
- `Source/Plugins/BuilderModes/VisualModes/BaseVisualMode.cs`, `BaseVisualMode`: modo visual especializado. **C:** editar superficie/objeto en 3D usando el mismo documento que el plano 2D.
- `Source/Core/Editing/GridSetup.cs`, `GridSetup`: grid/snapping como servicio compartido. **C/B:** magnitudes explícitas para movimiento, ángulo y escala; no valores duplicados por control.
- `Source/Core/Editing/UndoManager.cs`, `CreateUndo`, `WithdrawUndo`, `PerformUndo/Redo`: agrupación por origen/grupo/tag y almacenamiento de snapshots. **A:** un gesto produce una acción; historial con etiqueta y cancelación. No copiar política que borra redo al iniciar una grabación: VESTIGIO ya conserva redo al rechazar candidato.
- `Source/Core/General/MapManager.cs`, `SaveMap`, `SavePurpose.Testing`: salvar para prueba es un propósito distinto, con validación y generación de datos. **A/C:** documento editable, candidato probado y exportación distinguibles.

Inferencia UX: selección coherente, grid visible, inspector contextual, errores navegables y modo visual reducen cambios de contexto. No se realizó un estudio de usabilidad del ejecutable. No copiar representación Doom como requisito de la escena 3D ni migrar Studio a WinForms.

## EasyRPG Editor — proyecto, base de datos y mapa separados

Pregunta: ¿cómo compartir definiciones entre niveles sin serializar todo el juego dentro de cada mapa?

Fuentes: [EasyRPG Editor](sources-index.md#editor).

- `src/model/project.cpp`, `Project::load`, `loadDatabaseAndMapTree`, `loadMap`, `saveMap`: separa descubrimiento del proyecto, base de datos, árbol y mapas; usa lectores liblcf para formatos legacy/XML. **A/D:** proyecto contiene catálogos; nivel sólo instancias y referencias.
- `src/model/project_data.cpp`, `ProjectData`: agrupa base y árbol con ownership explícito. **A:** separar contenido global de escena activa.
- `src/model/event_command_list.cpp`, `EventCommandList::commands/command/index`: comandos de evento editables como datos. **D/C:** construir interacciones sin escribir código para cada objeto.
- `src/ui/map/undo_event.cpp`, `UndoEvent::undo`: restaura datos del evento por ID a la escena. **A:** ID estable y operaciones sobre modelo, no puntero a widget.

El repositorio estudiado es el editor; **no se auditó EasyRPG Player**. Por tanto, no se certifica su contrato completo editor/runtime ni se asume que el editor esté completo. Recuperar la organización de datos; no adoptar bases de combate JRPG ni binarios LCF para VESTIGIO.

## RPG Paper Maker — estados, reacciones y contenido accesible

Pregunta: ¿cómo describir objetos de un mundo 3D con sprites, estados y eventos editables?

Fuentes: [RPG Paper Maker](sources-index.md#rpg-paper-maker). El HEAD consultado incluye editor TypeScript/TSX; no se asume una arquitectura histórica distinta.

- `src/editor/models/MapObject.ts`, `MapObject::read/write`; `MapObjectState.tsx`, `MapObjectState`: objetos con bindings de serialización y estados editables. **D/C:** un esquema de propiedades reutilizable por inspector/serializador/validación.
- `src/editor/models/MapObjectReaction.ts`, `read/write/copy`: reacciones contienen listas de comandos y clonación. **D:** evento→condiciones→acciones, con argumentos y referencias; VESTIGIO ya tiene buena parte del runtime equivalente.
- `src/editor/core/Project.ts`, `load/save`: catálogos de imágenes, formas, canciones, variables, mapas, teclado y otros datos. **A/C:** biblioteca organizada por recurso y necesidades del creador. No copiar todo su catálogo de RPG.
- `src/editor/managers/UndoRedo.ts`, `applyStates`, `pendingSave`, `MAX_SAVES`: cambios before/after y almacenamiento por mapa. **A:** deltas identificables, exclusión de trabajo en curso y control de memoria. No adoptar I/O por cada gesto sin medir.

La EULA es propietaria; plugins poseen licencias individuales. No confundir repositorio visible con permiso para reutilizar código. La productividad de su workflow se infiere de modelos y herramientas, no de una sesión humana ejecutada aquí.

## Raylib PSX Odin — técnicas concretas en GPU

Pregunta: ¿qué efectos necesitan intervenir en geometría y cuáles pertenecen al framebuffer?

Fuentes: [Raylib PSX Odin](sources-index.md#raylib-psx-odin).

| Archivo / símbolo | Técnica encontrada | Evaluación |
|---|---|---|
| `src/main.odin`, `main` | `LoadRenderTexture(320,240)`, filtro POINT, modelos y shaders raylib | A/B: demuestra el mecanismo GPU que el adaptador actual no aprovecha |
| `shaders/ps1_vert.glsl`, `main`, `resolution` | Divide clip por w, cuantiza XY en NDC, vuelve a clip | B: snapping real de vértices; su jitter depende de cámara/geometría, no ruido aleatorio |
| Vertex/fragment, `noperspective fragTexCoord` | UV lineal en pantalla | B: interpolación afín auténtica dentro del triángulo; necesita material/vertex stage, no postprocess final |
| `ps1_frag.glsl`, `ditherMatrix`, `colorDepth` | Bayer 4×4 y cuantización a 31 intervalos | B: perfil visual de color; útil y barato, no emulación completa de PS1 |
| `fragFogDist = pos.z`, fog exponencial | Distancia en clip y factor fijo | No portar: no es distancia métrica estable; el comentario dice linear pero la fórmula es exponencial |
| `main`, destino de `DrawTexturePro` | Estira 320×240 a toda ventana | No portar: no conserva automáticamente aspecto 4:3 en ventana 16:9 |

PSX debe ser un perfil del backend GPU común. Baja resolución y nearest son restricciones de muestreo; snapping/affine cambian geometría/interpolación. Bloom, glitch, viñeta y aberración son tratamientos artísticos opcionales, no requisitos para PSX. No se copian shaders; licencia zlib permitiría evaluar reutilización con atribución/avisos, pero una versión propia parametrizada encaja mejor. El modelo del ejemplo tiene CC-BY-4.0 independiente y no se utiliza.

## Godot — respuestas puntuales, no plantilla de engine

Fuentes: [Godot](sources-index.md#godot).

| Pregunta | Archivo / símbolo | Idea aplicada |
|---|---|---|
| ¿Identidad resistente a renombres? | `core/io/resource_uid.h`, `ResourceUID::add_id/set_id/get_id_path` | D: AssetId separado de ruta; cambios de ruta no reescriben todas las instancias |
| ¿Cómo expresar caché/recarga? | `core/io/resource_loader.h`, `ResourceLoader::load`, `CACHE_MODE_REUSE/REPLACE` | A: política explícita; caché y ciclo de vida no se esconden en cada entidad |
| ¿Input configurable? | `core/input/input_map.h`, `InputMap::add_action/action_add_event` | D/A: acción semántica con varios dispositivos/bindings |
| ¿Pivot y jerarquía? | `scene/3d/node_3d.h`, `Node3D::set_transform/set_global_transform/reparent` | A: local/world y política al cambiar padre; nodo bisagra padre de panel |
| ¿Undo compartido? | `editor/editor_undo_redo_manager.h`, `create_action`, `commit_action` | A/C: acciones nombradas, contexto documental y merge explícito |

Se inspeccionaron esas interfaces, no todo Godot ni sus implementaciones internas completas. No se adopta scene tree como jerarquía universal de comportamiento, reflection completa, import database masiva o ecosistema de plugins. Para VESTIGIO bastan TRS jerárquico opcional, catálogo de recursos y esquemas pequeños.

## Síntesis aplicable

La convergencia útil es: datos de contenido independientes de instancia; recursos compartidos; consultas espaciales comunes; estados/eventos explícitos; comandos de editor reversibles; pipeline GPU por perfil; juego como consumidor del engine. Las diferencias históricas de lenguaje, formato y licencia hacen inadecuado ensamblar fragmentos de estos repositorios como arquitectura.
