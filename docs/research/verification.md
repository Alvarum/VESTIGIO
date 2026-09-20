# Verificación y cobertura de la investigación

Fecha: 2026-09-20. Esta entrega es investigación y planificación. **No implementa el backend GPU ni certifica que el engine actual ya lo tenga.** La decisión de usar GPU atraviesa arquitectura, SDK, recursos, viewport, pipeline y criterios de aceptación del roadmap.

## Evidencia y límites

- Baseline local: `4e0cb9c866dcb2aa59e0e12a333c600ca12a390d`, conservado como snapshot temporal para evitar mezclar cambios concurrentes. Delta posterior hasta `ae48d31e4be47e25a5d2e7eba5d2b541c335fc82` registrado en el anexo de [01](01-current-engine-audit.md#cambios-concurrentes-observados-al-cierre).
- Proyecto anterior: `C:/Users/alvar/Documents/dev/js-game/`, ubicación confirmada por el usuario; revisión `bca6f57af2805809e87674297a2555da3b6a3957`. Lectura y búsquedas dirigidas, sin modificarlo.
- Nueve repositorios externos: revisión selectiva según preguntas, archivos y símbolos de [03](03-reference-engines.md). El [manifest](source-manifest.json) fija revisión, URL y hash de los archivos consultados. No implica revisión integral de esos proyectos.
- Documentación auxiliar oficial: glTF, raylib/cgltf y WPF. Los textos de licencia se consultaron y las incertidumbres se registraron; no se copió código de referencia al motor.
- No se ejecutaron build, pruebas del motor, juegos externos, pruebas visuales ni benchmarks. Las pruebas existentes se identifican como evidencia de contratos escritos. Los bloques C son propuestas ilustrativas, no un SDK compilable; los JSON son ejemplos de un formato todavía no implementado.

## Cobertura del encargo

| Requisito / pregunta | Resultado localizable |
|---|---|
| Identidad de VESTIGIO, especialización retro y otros géneros | [06](06-target-architecture.md): runtime C, SDK y Studio; gamekit opcional |
| Arquitectura actual, dependencias, memoria, deuda y partes a conservar | [01](01-current-engine-audit.md): mapa, archivo/símbolo/responsabilidad, A01–A14 |
| Arqueología Three.js y conocimiento de producto | [02](02-threejs-engine-archeology.md): workflows, errores de round-trip/ownership, sistemas recuperables |
| Nueve referencias, categorías A–E y código real | [03](03-reference-engines.md), [índice de fuentes](sources-index.md) |
| Capability matrix y gaps | [04](04-capability-matrix.md), estados de evidencia definidos |
| Licencias, titular, procedencia y decisión | [05](05-source-provenance.md); [ruta alternativa solicitada](source-provenance.md) |
| GPU como capacidad principal y distribución CPU/GPU | [06](06-target-architecture.md), [09](09-render-pipeline.md), [10](10-asset-system.md) |
| Capas, engine/game, entidades, ECS y alternativas | [06](06-target-architecture.md), [07](07-public-c-api.md) |
| SDK externo C, static/dynamic/callbacks/plugins, lifecycle y errores | [07](07-public-c-api.md) |
| Editor-first y code-first sobre el mismo runtime | [07](07-public-c-api.md), [08](08-editor-architecture.md), aceptación P8 |
| Automatización, Tool API, CLI y MCP opcional | [06](06-target-architecture.md), [07](07-public-c-api.md), P8 |
| Editor, selección, TRS, undo, inspector, biblioteca, jerarquía y logs | [08](08-editor-architecture.md) |
| Bisagras, pivots, colisión, bloqueo, interacción y persistencia | [06](06-target-architecture.md), [11](11-level-format.md), P6 |
| Importación de modelos, ejes, escala, materiales y animación | [10](10-asset-system.md), P3/P7 |
| Shaders, Neo-PSX, affine, snapping, dither, fog y luces | [09](09-render-pipeline.md), investigación Odin/GZDoom |
| Resolución interna, upscale, aspecto y settings | [09](09-render-pipeline.md), ejemplos numéricos y contrato de superficie |
| Asset manager, handles, caché, unload, audio y reload | [10](10-asset-system.md) |
| Proyecto, mapas 3D, JSON/binario, migraciones, save y export | [11](11-level-format.md) |
| Debug, profiling, RAM/VRAM y evidencia GPU | [08](08-editor-architecture.md), [09](09-render-pipeline.md), P7/P8 |
| Roadmap con dependencias, aceptación, riesgos, deuda y resultados | [12](12-roadmap.md), P0–P8 |

## Comprobación documental

La comprobación final valida presencia de los doce documentos, destinos de enlaces Markdown locales, bloques de código cerrados, sintaxis de ejemplos JSON, estructura del manifest y hashes de las fuentes contra las copias consultadas. Los enlaces web de código quedan fijados por commit; no se presenta una comprobación HTTP de cada enlace como prueba del contenido o comportamiento.

Resultado del verificador documental: **0 errores**; 12 documentos requeridos presentes, 16 archivos Markdown en total, 102 enlaces locales resueltos (15 con ancla), 2 ejemplos JSON válidos, 64 archivos externos de 9 repositorios y 92 archivos de evidencia local con SHA-256 coincidente. Los Markdown se leyeron como UTF-8, sin bloques sin cerrar ni espacios sobrantes al final de línea. Esta validación se limita a integridad documental y trazabilidad; no demuestra por sí misma la corrección de todas las conclusiones arquitectónicas.

El alcance de escritura de esta entrega es `docs/research/`. Los cambios concurrentes del motor pertenecen a otros trabajos y se preservaron. Antes de implementar, P0 reconcilia el HEAD entonces vigente y P1 exige demostrar render GPU real e integración de superficie; leer esta investigación no satisface esos criterios.
