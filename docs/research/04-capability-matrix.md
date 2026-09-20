# 04 — Matriz de capacidades y decisiones

Esta matriz compara **evidencia encontrada**, no calidad global ni versiones comerciales. `C` = código/contrato localizado; `P` = parcial o especializado frente a lo requerido por VESTIGIO; `I` = interfaz/datos localizados, integración no auditada; `—` = no evaluado en esa referencia; `N` = no localizado en la búsqueda dirigida del baseline local. Ningún `C` significa prueba runtime ejecutada.

Columnas: V = VESTIGIO `4e0cb9c`, J = js-game confirmado, F = FPS Creator, D = DOOM, Q = Quake, GZ = GZDoom, GB = GZDoom Builder, ER = EasyRPG Editor, RP = RPG Paper Maker, GO = Godot, OD = Raylib PSX Odin. Evidencia/símbolos de V en [01](01-current-engine-audit.md), J en [02](02-threejs-engine-archeology.md), referencias en [03](03-reference-engines.md) y [fuentes](sources-index.md).

El [anexo del delta hasta ae48d31](01-current-engine-audit.md#cambios-concurrentes-observados-al-cierre) registra imágenes de materiales, una corrección parcial de OOM, atlas y texto/animación posteriores. Las celdas no pretenden certificar un HEAD distinto del baseline.

| Capacidad | V | J | F | D | Q | GZ | GB | ER | RP | GO | OD | Decisión VESTIGIO |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Mismo runtime para editor/juego | C | P | P | — | — | — | P | I | I | — | — | Preservar sesión común; extender a código propio |
| API C externa estable | P | — | — | P | P | — | — | — | — | — | — | SDK estático primero; fachada C mínima |
| Separación engine/game | P | P | P | P | C | P | — | — | — | — | — | Callbacks y game module incorporado reemplazable |
| Render GPU de geometría | N | C | — | — | — | C | — | — | — | — | C | **Backend principal GPU**, petición explícita del usuario |
| Framebuffer interno + upscale | C fijo | P | — | — | — | — | — | — | — | — | P | Configurable, aspecto y escala; GPU sin readback por frame |
| Mundo 3D con mallas | P triángulos | C | I modelo | P sectores | C BSP | P heredado | P mapas | — | I objetos | I Node3D | C demo | Escena 3D, mallas/colisión, legacy como adaptador |
| Sprites en mundo 3D | C | — | — | C | I modelo | I actor | — | — | I | — | — | SpriteRenderer junto a MeshRenderer |
| Importar GLB/glTF | N | C | — | — | — | — | — | — | — | — | C | Prioridad temprana, importer CPU y upload GPU |
| Caché de recursos compartidos | P | P | — | — | C | C shader | — | — | I catálogo | I | P | AssetId estable + handles + dependencias |
| TRS y jerarquía espacial | N general | C | P offsets | P | P | P | P | — | I | I | P | Transform opcionalmente jerárquico, sin jerarquía de clases |
| Puerta con bisagra | N | P | P animación | P vertical | — | — | — | — | — | I transform | — | Componente hinge + collider cinemático barrido |
| Colisión/sweep/raycast | C 2.5D | P 3D | P puerta | C 2.5D | C 3D | — | — | — | — | — | — | Consultas comunes, static mesh y dinámicos |
| Actores/estados/armas | C parcial | C parcial | P FPI | C | C frontera | C | — | I base | I estados | — | — | Helpers opt-in; núcleo sin enemigos obligatorios |
| Interactuables/eventos/triggers | C | P persistencia | C | C | C spawn | I actor | — | I eventos | I reacciones | — | — | Completar authoring y codecs existentes |
| Luces editables | P | P exportación | — | P sector | — | C GLDefs | — | — | — | — | — | Datos world, shader GPU, límites explícitos |
| Fog de mundo configurable | N | P especializado | — | — | — | — | — | — | — | — | P fijo | Fog lineal/exponencial en unidades de mundo |
| Shaders por material | N | C | I effect | — | — | C | — | — | — | — | C | GLSL versionado, slots y metadata editable |
| Snapping/affine PSX | N | N en PS1Shader | — | — | — | — | — | — | — | — | C | Vertex/material stages GPU; no sólo filtro final |
| Dither/cuantización | N | C | — | — | — | — | — | — | — | — | C | Perfil parametrizado y preset desactivable |
| Editor 3D/gizmos/selección | P preview | C | P | — | — | — | C | — | I | I | — | Viewport GPU + picking + herramientas |
| Inspector/biblioteca/properties | P | C | C FPE | — | — | Datos | C | I | I | — | — | Esquemas/IDs y selectores de referencias |
| Undo/redo transaccional completo | C acotado | P TRS | — | — | — | — | C | P evento | C deltas | I | — | Preservar comando candidato, extender cobertura |
| Plantas/huecos/world building | P | P | P | P | C BSP | — | C geometría | — | I mapas | — | — | Recetas editables con geometría/collider derivados |
| Música/audio ambiental/posicional | P PCM | C parcial | I soundset | — | C host | — | — | — | I catálogo | — | — | AudioAsset + Voice + buses; streaming |
| Settings/input mapping | P fijo | P | I config | — | C CVars | C CVars | — | — | I keyboard | I InputMap | P fijo | Registro tipado + bindings comunes entre hosts |
| Proyecto/mapa versionado | C | P divergente | C | C | C | — | C | C | C | I recursos | — | JSON autoría + legacy + binario derivado opcional |
| Guardados/estado de juego | P sesión | P | — | — | — | — | — | — | — | — | — | Contrato por componente, distinto del documento |
| Consola/profiling/debug | P | I herramientas | — | P | C | C CVars | I | — | — | — | P FPS | CPU/GPU/VRAM y consultas; no sólo FPS |
| Automatización/CLI | P flags | P scripts | — | — | C comandos | C consola | — | — | — | — | — | Tool API transaccional, CLI después; MCP opcional |

`Datos` en GZ indica definición de contenido inspeccionada, no inspector visual. Para evitar una falsa conclusión de ausencia, las referencias sólo marcan `N` donde hubo búsqueda dirigida local; un guion nunca significa que el producto carezca de la capacidad.

## Gaps que gobiernan el orden

1. **GPU + superficie de presentación**: debe probarse temprano, incluida la frontera con WPF. La copia actual de píxeles no define el futuro render.
2. **Identidad + recursos + transforms**: prerrequisitos comunes de modelos, editor y API; deben crecer juntos en una vertical mínima, no esperar al asset manager perfecto.
3. **Codec único y documento transaccional**: la arqueología muestra pérdidas de datos aun cuando los widgets existen.
4. **Colisión 3D y puertas**: visualizar un modelo no lo convierte en mundo transitable; el hinge no se entrega sin collider actualizado.
5. **Gameplay extensible**: `re_session_frame` no permite sustituir las reglas incorporadas; el SDK debe demostrar un segundo juego independiente.
6. **Estado de recursos GPU**: caché de RAM, residency en VRAM, contexto gráfico y sincronización deben tener ownership explícito.

Las primeras entregas deben ser dos rutas al mismo resultado: importar/colocar/guardar/jugar desde Studio y crear/cargar la misma escena desde un proyecto C externo. Esto mide identidad del engine, no sólo cantidad de features.
