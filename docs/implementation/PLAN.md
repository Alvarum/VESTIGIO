# Plan ejecutable de VESTIGIO

Fecha: 2026-09-20. Base observada: `28dafa949ff68ed3dc52bf93d287863037bf8578`, checkout limpio al comenzar esta planificación. El diff de código entre `ae48d31` y esta base es vacío; el nuevo commit incorpora la investigación. F00 debe comprobar el HEAD real cuando otro agente empiece, porque esta observación no congela el repositorio.

## Resultado que se implementará

Un runtime retro 3D en C con renderer GPU, SDK externo y Studio WPF como editor del mismo contenido. Gamekit aporta helpers opcionales FPS/horror; un juego de exploración sin armas debe funcionar sin modificar internals. Código y editor pueden generar el mismo nivel; ambos conservan datos al guardar, reabrir y jugar.

Se implementa por verticales: primero GPU/superficie y contratos mínimos; después modelos compartidos/documento; después autoría/colisión; finalmente ambiente/animación/audio y entrega. No se completa un subsistema entero antes de probarlo con su consumidor real. Las fases P0–P8 del informe siguen siendo la justificación; los tickets concretan el orden ejecutable y evitan dependencias implícitas.

## Tramos que se pueden encargar

| Encargo | Tickets / cierre | Resultado y límite |
|---|---|---|
| ARRANQUE, recomendado inicialmente | F00, F01, G01, G02, R01, R02, R03, I01 | GPU real y superficie de Studio probada, runtime/ownership/SDK/input mínimo. Modelos importados y autoría completa siguen pendientes |
| BASE_3D | Anteriores + D01, M01, G03, G04, D02, D03 | Modelo GLB compartido GPU, legacy renderizado, nivel canónico y SDK externo con escena real |
| CREATOR | Anteriores + E01–E05, S01–S03, V01–V02, A01–A02 | Crear/importar/editar/guardar/reabrir/jugar con puertas, colisión, luces, fog, audio y animación |
| COMPLETO | Todos, incluyendo Q01, P01, P02, T01, Z01 | Dos juegos, SDK/exportación independiente, partidas y CLI con revisión; aceptación integrada |

Los nombres de tramos son selecciones de tickets, no nuevos estados ni saltos de dependencias. Un encargo menor no autoriza ejecutar tickets fuera de él; un encargo COMPLETO sí permite continuar autónomamente por las dependencias integradas. Si se entrega únicamente un ticket, el agente no implementa sus predecesores por su cuenta: verifica que ya estén integrados.

## Primer recorrido de implementación

1. **F00:** medir baseline, revisar cambios y preparar evidencia; no basarse en PASS históricos. Las reparaciones necesarias se acotan y registran antes de asumirlas.
2. **F01:** fijar contratos v0.1 y archivos de integración; sólo declaraciones necesarias y pruebas de consumo del header.
3. **G01 ∥ R01:** GPU en escena sintética mientras se construyen entidades/Transform sin GPU. Se comparten contratos, no archivos de implementación.
4. **G02 ∥ R02 → R03:** resolver hosting en WPF y completar assets/SDK. Los cambios de CMake/API se integran secuencialmente con responsable único.
5. **I01:** paridad de input y settings entre hosts. Puede esperar a G02 si ambos necesitan los mismos archivos de viewport/plataforma.
6. Integrar ambos carriles, ejecutar aceptación de ARRANQUE y entregar estado. No afirmar que esa vertical ya permite crear un juego completo.

Para BASE_3D, D01 y M01 son independientes una vez cumplidas sus dependencias; G03 une GPU/importación/SDK, G04 conserva niveles anteriores y D03 prueba la migración real. En CREATOR, editor, física y presentación tienen trabajo independiente, pero E05 exige que todo se encuentre en el mismo candidato.

La [lista de oleadas](WAVES.md) calcula el mínimo teórico por dependencias. **Dos tickets de una misma oleada sólo pueden ejecutarse simultáneamente si no comparten locks ni archivos reales.** No iniciar toda una oleada a ciegas.

## Un agente

Elige el primer ticket elegible dentro del alcance encargado. Implementa, prueba, integra y actualiza estado antes de pasar al siguiente. Cuando haya varias opciones, prioriza GPU/superficie, luego el consumidor que cierre una vertical. No sustituir implementación por nuevos planes salvo que aparezca un bloqueo técnico real que requiera decidir.

No necesita simular cinco personajes ni crear ramas por rutina. Sí debe mantener alcance por ticket, revisión de diff, evidencia y checkpoints que permitan a otro agente retomar sin releer toda la conversación.

## Varios agentes

Responsabilidades lógicas, no obligación de mantener cinco agentes activos:

| Rol | Trabajo principal | Frontera que debe respetar |
|---|---|---|
| integrator | F00/F01/P02/Z01, contratos, build, secuencia y aceptación | Único escritor de estado global/backlog y rama integrada |
| gpu | Backend, superficie, upload/materiales/shaders/perfiles y profiling | Contexto/gráficos privados; API pública no expone raylib |
| runtime | Handles/world, assets base, callbacks/input, física/gamekit/audio | No edita documentos desde el tick ni serializa memoria arbitraria |
| content | Importación, formato, migración, animación, savegame y CLI | IR/IDs compartidos; no introduce otro serializer ni carga GPU desde parser |
| editor | Viewport, herramientas, inspector y recorrido integrado | Usa documento nativo y esquemas; no posee una segunda verdad del nivel |

Con dos agentes: integrador/runtime + GPU primero; luego redistribuir contenido/editor. Con tres: integrador, GPU/editor y runtime/contenido. Con cuatro: integrador más tres workers; reasignar rol cuando un ticket termine. No crear agentes ociosos ni superar la concurrencia realmente disponible.

El coordinador reclama ticket y locks antes de delegar, indica base commit, rutas exactas de escritura y contrato vigente. Los workers no modifican backlog/STATE ni integran otros tickets. Devuelven commits o diff, pruebas, riesgos e instrucciones de integración. El coordinador libera locks sólo al integrar o devolver la tarea.

`paths` en backlog son puntos de entrada y áreas permitidas a concretar, incluyendo archivos para lectura; no autorizan editar indiscriminadamente todos los archivos de un directorio. Antes de empezar, cada worker declara su lista de escritura. Cualquier solapamiento real bloquea simultaneidad aunque falte en los locks iniciales. F00 sólo escribe evidencia/estado salvo reparación adicional explícitamente acotada.

## Aislamiento y archivos compartidos

- Preferir worktrees/checkouts aislados por worker, desde un commit integrado que contenga sus dependencias. No usar reset/clean/restore global ni mover trabajo ajeno para preparar una rama.
- Cada checkout usa sus propios build/bin/obj/artefactos. `tools/build.ps1` y presets calculan rutas desde sourceDir: un worktree nuevo no trae `.tools`, `.deps` ni `.nuget` ignorados. Comprobarlo antes de lanzar builds; no fingir que bootstrap ya ocurrió.
- Reutilizar herramientas/cachés sólo si el coordinador establece rutas y acceso seguro; nunca compartir salidas de compilación. Si no es posible preparar entornos aislados, usar un único escritor y serializar implementación en el checkout disponible.
- `tools/bootstrap.ps1` actualiza paquetes locales además de restaurarlos. No ejecutarlo concurrentemente sobre una caché compartida ni sólo por rutina; F00 decide si hace falta y registra versiones.
- Cambios en `CMakeLists.txt`, presets, `include/vestigio/`, esquema común y bridges de sesión se reclaman con locks. Un worker puede preparar un cambio en su rama, pero el coordinador revisa e integra de uno en uno. No hay export masivo de símbolos nuevos para evitar diseñar la API.
- Las pruebas que escriben en proyectos existentes trabajan sobre copias aisladas del corpus. No sobrescribir Haunted original, partidas del usuario ni evidencia de otro ticket.

## Estados y entrega por ticket

`PLANNED` → `IN_PROGRESS` → `IMPLEMENTED` → `VERIFIED` → `INTEGRATED`. `BLOCKED` exige motivo y condición concreta de desbloqueo. Elegibilidad se calcula: todas las dependencias INTEGRATED, ticket dentro del encargo, locks libres y writer set disjunto.

IMPLEMENTED indica código escrito; VERIFIED indica aceptación del ticket comprobada en su rama; INTEGRATED exige revisión, incorporación al candidato y checks afectados en ese candidato. Un screenshot, header compilable o test sin comportamiento no permite saltar estados. Si un entorno no permite una comprobación requerida, registrar `NOT_RUN` y no marcar cumplimiento por inferencia.

Entrega mínima del worker: ID, base/result commit, archivos, cambio/razón, comandos y salidas, casos de aceptación, artefactos, límites y siguiente dependencia. Usar [el formato de handoff](AGENT-PROMPTS.md#entrega-de-un-worker). Mantener evidencia por ticket/commit y referencia en STATE; nunca copiar resultados de una rama como si provinieran de otra.

## Decisiones y bloqueos

Una decisión local dentro del contrato corresponde al agente; no necesita pedir permiso por cada función o test. Cambios de frontera se resuelven con nota breve: problema reproducido, dos alternativas relevantes, coste, elección, tickets afectados y migración. Actualizar contrato y DAG antes de continuar dependencias.

G02 es el principal riesgo inicial: ventana externa demuestra GPU, pero no el editor embebido. Si falla, conservar experimento/evidencia, bloquear E01 y continuar lo independiente autorizado. La solución no puede ser readback continuo escondido ni reescribir todo Studio sin evaluar coste.

No implementar por anticipación networking, ECS universal, editor de shaders por nodos, game DLL/hot reload, scripting, bindings múltiples, Vulkan/D3D12 propios, PBR completo o MCP. Todo queda fuera de estos tickets salvo instrucción posterior. La licencia de código propio pendiente no bloquea investigación/implementación interna ordinaria; sí impide declarar autorizada una publicación pública bajo una licencia inventada.

## Fin del encargo

Al concluir el alcance asignado, entregar resultados reales y el siguiente ticket elegible. No avanzar a un tramo adicional por suposición. Si el usuario autorizó COMPLETO, sólo cerrar tras Z01 o explicar bloqueos verificables con trabajo independiente agotado. Este plan por sí solo no autoriza despliegues, envíos a terceros o publicación remota.
