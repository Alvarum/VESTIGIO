# 02 — Arqueología de js-game

Ruta confirmada por el usuario: `C:/Users/alvar/Documents/dev/js-game/`. La ruta original `js-game.planning` era incorrecta; `.planning` está dentro de `js-game`. HEAD consultado: `bca6f57af2805809e87674297a2555da3b6a3957`. Había tres resúmenes de planificación no versionados; no se usaron como prueba de implementación.

Evidencia: lectura de código, no ejecución del navegador ni aceptación visual. Cada ruta de esta página es relativa a ese repositorio. El código fuente prevalece sobre comentarios y README. No se porta JavaScript a C ni se incorporan assets.

## Identidad encontrada

El repositorio combina un framework de horror/exploración (`engine/`), La Casa (`game/apartment/`) y herramientas in-game. `Entity` envuelve `THREE.Group`; otros sistemas crean objetos estructuralmente compatibles sin heredar de `Entity`. `SceneManager.register/goto/update` compone escenas mediante fábricas y contexto. `SceneEditor` utiliza entidades/meshes reales como estado editable, construye DOM y manipula `TransformControls`.

La idea a recuperar es **crear una experiencia colocando contenido y enlazando acciones**, con feedback inmediato. Su principal debilidad es que escena visual, datos persistentes y registros de física/interacción no forman una transacción única.

## Sistemas recuperables

| Sistema / archivo / símbolo | Problema y diseño encontrado | Lo bueno | Lo malo / límite | Adaptación y qué no portar |
|---|---|---|---|---|
| `engine/SceneEditor.js`, `mount`, `_select`, `_toggleSpace`, `_toggleSnap`, `_snapToFloor` | Selección por raycast, gizmo, espacio local/global y encaje al suelo | Edición directa; menos números manuales | UI, carga, historial y persistencia reunidos en un archivo de ~68 KB | Conservar gestos; herramientas pequeñas sobre Tool API, sin DOM ni referencias a meshes como identidad |
| `SceneEditor._loadManifest`, `_populateBrowser`, `_spawnModel` | Browser agrupado por modelos, spawn frente a cámara | Cierra descubrir→colocar→ajustar | Carga directa GLTF y rutas dependientes del proyecto | Asset browser sobre catálogo estable; placeholder y progreso verificables |
| `SceneEditor._showPackDialog`, `_explodePack`, `_spawnGltf` | Importar un GLB completo o elementos de un pack | Descubrimiento importante: selección de submodelos | Riesgo de asumir que nombre de nodo es identidad estable | IDs de subrecurso y preview de jerarquía; mantener archivo fuente completo |
| `SceneEditor._pushUndo/_undo/_redo` | Snapshots de posición, rotación y escala; límite 20 | Reversibilidad de transformaciones | Guarda referencias a entidades; no representa creación/eliminación/materiales como operaciones completas | Mantener undo de VESTIGIO, añadir comandos por ID; no portar esta pila |
| `SceneEditor._export/_autosave/_restoreSaved`; `SceneLoader.load` | Export scene.json y recuperación localStorage | Persistencia legible y recuperación rápida | Export/import divergentes; autosave parcial y fallo silenciado | Un serializador canónico, validación y round-trip; autosave separado del archivo autorizado |
| `engine/AssetLoader.js`, `loadTexture`, `loadGLTF`, `_pending`, `dispose` | Caché por clave y promesas compartidas | Evita repetir una solicitud por clave, progreso | Clave no garantiza igualdad de URL/opciones; callbacks pendientes no cancelados; `gltf` no tiene dispose global | Assets inmutables con dependencias/handles; tabla de solicitudes en vuelo si se añade async |
| `engine/SceneLoader.js`, `load`, `unload` | Reconstruye props, placeholders y registro de interacción | Errores visibles mediante placeholder; metadatos de plantas | Omite AssetLoader, carga cada prop; `unload` sólo retira grupos | Instanciar del recurso compartido; destroy desregistra todos los sistemas |
| `engine/RoomBuilder.js`, `_finishRoom`, `_buildWall`, `_buildFloorCeiling` | Dibujo de contorno y generación de paredes/suelo/techo | Construcción espacial sin DCC externo | Geometría procedural y datos exportados no tienen contrato completo común | Conservar receta paramétrica editable y derivar mesh/collider; no exportar sólo etiqueta |
| `engine/WallHoleTool.js`, `applyHoleTo`, `_buildHolePieces`, `_placeModel` | Recortar huecos, marcos y colocar puertas/ventanas | Workflow muro→hueco→marco→objeto | Destruye/reemplaza geometría del mesh y requiere sincronización externa | Operación documental atómica con geometría y collider derivados; rediseñar internamente |
| `engine/LightPlacer.js`, `_placeLight` | Colocar luces con parámetros y representación visual | Preview espacial de point/spot y configuración contextual | Dependencias directas escena/renderer/listas | Componente Light y gizmo; inspector generado por esquema |
| `engine/TriggerZonePainter.js`, `_placeZone`; `TriggerZone.js` | Volúmenes con entrada/salida, once y cooldown | Herramienta espacial explícita; eventos nombrados | Metadatos `_once`, `_cooldown`, etc. no bastan para persistencia/runtime | Reutilizar triggers de VESTIGIO y completar authoring/serialización |
| `engine/FloorManager.js`, `setActiveFloor`; `SceneEditor._applyGhostMode` | Organizar plantas y ocultar/atenuar otras | Editar por contexto, evitar seleccionar otro piso | Runtime oculta props de otros pisos: incorrecto para atrios/escaleras visibles | Capas/plantas como organización de editor, no regla universal de visibilidad |
| `game/apartment/entities/Door.js`, constructor, `update`, `resolveCapsule` | Bisagra como Group en borde, panel hijo desplazado, giro progresivo | Pivot separado de centro de malla; sentido de apertura | Dimensiones/velocidad fijas; collider se desactiva cerca de apertura total; no sweep angular | Adaptar concepto de hinge + collider kinemático; rediseñar estados, bloqueos y colisión |
| `engine/PhysicsWorld.js`, `addGeometry`, `setDynamicColliders`; `FPSController.js` | Octree estático y colliders dinámicos consultados aparte | Separar geometría inmóvil de puertas móviles | API expone Octree; mover mesh no demuestra actualización de colisión | Consultas abstractas con capas; registro de cambios y actualización acotada |
| `engine/InteractionSystem.js`, `register`; `Entity.actions` | Raycast selecciona objetos, acciones contextuales | Intención de usuario y estado bloqueado visibles | Funciones JS/objetos directos no son serializables | IDs de acciones y argumentos tipados; callbacks C para ejecución |
| `engine/ScriptEngine.js`, `addHandler`, `run`, `stop` | Secuencias async de narrativa y handlers extensibles | Eventos reutilizables, waits y composición | Promesas/timers no equivalen a estados de juego guardables | Máquina de secuencias data-driven sobre tick; no introducir VM todavía |
| `engine/SceneManager.js`, `register`, `goto` | Cambios de escena por fábrica/contexto | Frontera engine/game sencilla | Destruye escena antes de validar fábrica/carga; no espera carga async | Cargar candidato y cambiar en frontera de tick; rollback si falla |
| `engine/PostProcessor.js`, `init`, `render`; `shaders/PS1Shader.js`, `PS1Shader` | Composer ordenado, horror reactivo, color reducido al final | Parámetros/presets y orden de passes explícito | Tamaños dependientes de canvas/pixel ratio; eventos/timers ligados al entorno | Pipeline por perfil y reloj; UI separada del postprocess; no portar pila completa |
| `engine/AudioWorld.js`, `playAt`, `_decode`, `update` | Fuentes espaciales con caché de audio y listener | Sonidos ubicables, vida de fuente separada de buffer | No constituye política unificada de ownership con AssetLoader | Sound compartido + Voice temporal + AudioEmitter + buses |
| `engine/SaveManager.js`, `save`, `load`, `_migrateV1toV2`, `_checksum` | Slots, versión, checksum y migración | Reconoce que datos guardados evolucionan | No confundir checksum con seguridad; localStorage y estado arbitrario JS | Conservar integridad/versionado de VESTIGIO y esquema explícito por componente |

## Fallos concretos que informan el nuevo diseño

1. **Round-trip roto en transformaciones.** `SceneEditor._export` escribe `position: g.position.toArray()`, `rotationDeg: [...]`, `scale: g.scale.toArray()`. `SceneLoader.load` lee `prop.position.x`, `prop.rotationDeg.x`, `prop.scale.x`. Sus defaults convierten un array en posición cero, rotación cero y escala uno. La incompatibilidad es demostrable por lectura; no se ejecutó una escena para medir el efecto visual.
2. **Datos que desaparecen al exportar.** `SceneLoader` admite v3 `floors`, `floorHoles`, `_floorIndex`; `_export` genera v2 sin esos campos. Luces/triggers/geometría procedural poseen propiedades que ese export genérico tampoco describe completamente. El objetivo debe ser cerrar todos los codecs, no sumar otra herramienta aislada.
3. **Caché existente pero no transversal.** `AssetLoader` centraliza cargas; `SceneLoader` tiene su propio `GLTFLoader` y llama `_loader.load(prop.modelFile)` por instancia. `SceneEditor` también usa `_gltfLoader`. No afirmar que el engine ya deduplica todos los modelos.
4. **Ownership incoherente al compartir.** `Entity.destroy` hace dispose de geometrías/materiales de cada mesh; eso entra en conflicto con compartirlos entre clones. `SceneLoader.unload` no dispone ni desregistra interacción. `AssetLoader.dispose` no recorre dependencias de glTF ni cancela callbacks en vuelo. Rediseñar este contrato desde cero en C.
5. **Comentarios obsoletos.** Door dice que las puertas son traspasables, pero implementa `resolveCapsule`; `FPSController` consulta `PhysicsWorld.dynamicColliders`. La integración de cada escena debe comprobarse por registro real. No usar el comentario antiguo para concluir que falta toda colisión.
6. **Historial parcial.** Restaurar TRS de una referencia no restaura una entidad eliminada ni sus registros físicos. No reemplazar el documento transaccional actual por ese mecanismo.

## Capacidades adicionales descubiertas

El inventario `engine/` contiene también `AnimationController` (mixer/clip), `CameraDolly`, `CarrySystem`, `DecalSystem`, `MusicDirector`, `ReverbZones`, `PuzzleManager`, `Timeline`, `PlayerVitals`, `FootstepSystem`, `Flashlight`, `Weapon`, `StateMachine`, `TouchInput`, `UIFocusManager`, `VideoPlayer`, `MirrorSystem`, `Ragdoll`, efectos de vidrio/fuego/carne y `effects/ExteriorFog`. Son **candidatos localizados**, no todos auditados end-to-end.

Se inspeccionaron métodos de `AudioWorld`, `ScriptEngine`, `SaveManager`, herramientas espaciales y `ExteriorFog`. Esta última construye varias capas de niebla/partículas con materiales especializados (`_buildMistFields`, `_buildStackedMistVolume`, `_buildFogFilaments`, `update`, `dispose`): recuperar ambiente como parámetros, no convertir esa escena artística concreta en el fog básico del engine.

Prioridad de recuperación: (1) colocación de modelos y bisagra, (2) edición directa con inspector y tool modes, (3) huecos/plantas como recetas, (4) audio ambiental y secuencias. Carry, ragdoll, espejos y efectos orgánicos quedan para después de un pipeline de recursos y niveles completo. Haberlos encontrado no los convierte en requisitos del primer release.

## Criterio de éxito heredado

Un objeto creado visualmente debe sobrevivir guardar→cerrar→abrir→jugar, con transform, materiales, colisión, propiedades y acciones equivalentes a los creados por C. La arqueología demuestra por qué esa prueba integrada vale más que contar herramientas del editor.
