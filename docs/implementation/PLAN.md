# Plan ejecutable de VESTIGIO

> **Ejecución por oleadas revisables (2026-09-27).** El alcance, orden y puertas de prueba están en [ENTREGABLES-HOBBY.md](ENTREGABLES-HOBBY.md). Las oleadas 0–4 tienen flujo funcional implementado; el usuario revisó J01 y S01/S02. El usuario encargó ejecutar consecutivamente las oleadas 4–7, con pruebas, commit, evidencia y push separados por cada una; luego se detiene. `WAVES.md` muestra dependencias técnicas, no oleadas de producto.

Fecha: 2026-09-20. Base observada: `28dafa949ff68ed3dc52bf93d287863037bf8578`, checkout limpio al comenzar esta planificación. El diff de código entre `ae48d31` y esta base es vacío; el nuevo commit incorpora la investigación. F00 debe comprobar el HEAD real cuando otro agente empiece, porque esta observación no congela el repositorio.

## Separación de motores y build actual

El plan implementa **VESTIGIO solamente**. Su fuente y contenido residen en
`engines/vestigio/`; RetroForge está aislado en `engines/retroforge/` y no es
una capa compartida ni una obligación de compatibilidad para este plan. Cada
carpeta de motor tiene su propio `CMakeLists.txt`, `src/`, `include/`, `tests/`,
`studio/`, `studio.tests/` y `assets/`. La raíz conserva el dispatcher CMake,
presets por motor y herramientas/cachés comunes.

Desde la raíz, use los wrappers estables y elija el motor de forma explícita:

```powershell
./tools/bootstrap.ps1
./tools/build.ps1 -Engine Vestigio -Preset debug -Test
./tools/build.ps1 -Engine RetroForge -Preset debug -Test
```

Los binarios quedan en `build/vestigio/debug/bin/` o
`build/retroforge/debug/bin/`. El build de un motor no incluye el otro. La
alternativa CMake directa usa los presets `vestigio-debug` y
`retroforge-debug`; esas salidas van a `build/workspace/` y también configuran
un solo motor por árbol. No reutilice el mismo directorio CMake para ambos.

## Resultado que se implementará

Un runtime retro 3D en C con renderer GPU, SDK externo y Studio WPF como editor del mismo contenido. Gamekit aporta helpers opcionales FPS/horror; un juego de exploración sin armas debe funcionar sin modificar internals. Código y editor pueden generar el mismo nivel; ambos conservan datos al guardar, reabrir y jugar.

Se implementa por verticales: primero GPU/superficie y contratos mínimos; después modelos compartidos/documento; después autoría/colisión; finalmente ambiente/animación/audio y entrega. No se completa un subsistema entero antes de probarlo con su consumidor real. Las fases P0–P8 del informe siguen siendo la justificación; los tickets concretan el orden ejecutable y evitan dependencias implícitas.

## Tramos que se pueden encargar

| Encargo | Tickets / cierre | Resultado y límite |
|---|---|---|
| ARRANQUE, recomendado inicialmente | F00, F01, G01, G02, R01, R02, R03, I01 | GPU real y superficie de Studio probada, runtime/ownership/SDK/input mínimo. Modelos importados y autoría completa siguen pendientes |
| BASE TÉCNICA (integrada) | ARRANQUE + D01, M01, G03, D02 | Modelo GLB compartido GPU, documento y SDK externo; aún sin escena recorrible |
| PRIMERA ESCENA (integrada y revisada) | J01 | Proyecto nuevo visible y recorrible libremente en Player con GPU real |
| MOVIMIENTO CON COLISIÓN (integrada y revisada) | S01–S02 | Consultas espaciales, movimiento, salto y colliders visibles en Player |
| STUDIO 3D (integrada; revisión humana pendiente) | E01 | Atrium GPU en Studio; cámara editorial y Editar/Probar aislados |
| EDICIÓN HOBBY (flujo funcional; E02/E03 incompletos) | E02–E03 parcial | Añadir y transformar pilar, deshacer/rehacer, guardar/reabrir y probar |
| AMPLIACIONES (opcionales) | E02–E05, S03, V01–V02, A01–A02 | Autoría, interacción y medios sólo por encargo individual |
| DISTRIBUCIÓN (opcional) | Q01, P01, P02, T01, Z01 y dependencias | Dos juegos exportables, partidas, CLI y aceptación integrada sólo si se decide llegar ahí |

La ejecución actual se rige por los entregables individuales de [ENTREGABLES-HOBBY.md](ENTREGABLES-HOBBY.md). El motor es nuevo: compatibilidad con niveles/proyectos anteriores y migración no forman parte del alcance. Si se entrega únicamente un ticket, el agente verifica que sus predecesores ya estén integrados.

## Primer recorrido de implementación

1. **F00:** medir baseline, revisar cambios y preparar evidencia; no basarse en PASS históricos. Las reparaciones necesarias se acotan y registran antes de asumirlas.
2. **F01:** fijar contratos v0.1 y archivos de integración; sólo declaraciones necesarias y pruebas de consumo del header.
3. **G01 ∥ R01:** GPU en escena sintética mientras se construyen entidades/Transform sin GPU. Se comparten contratos, no archivos de implementación.
4. **G02 ∥ R02 → R03:** resolver hosting en WPF y completar assets/SDK. Los cambios de CMake/API se integran secuencialmente con responsable único.
5. **I01:** paridad de input y settings entre hosts. Puede esperar a G02 si ambos necesitan los mismos archivos de viewport/plataforma.
6. Integrar ambos carriles, ejecutar aceptación de ARRANQUE y entregar estado. No afirmar que esa vertical ya permite crear un juego completo.

La base técnica, J01 y S01/S02 ya están integrados. El usuario revisa ahora el movimiento con colisión en Player. Editor y presentación son ampliaciones optativas; E05 exige que las funciones encargadas se prueben en un mismo candidato cuando se llegue a ese hito.

La [lista de oleadas técnicas](WAVES.md) calcula el mínimo teórico por dependencias. **Dos tickets de una misma fila sólo pueden ejecutarse simultáneamente si no comparten locks ni archivos reales.** Esa fila nunca amplía la oleada de producto encargada.

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
| content | Importación, formato, animación, savegame y CLI | IR/IDs compartidos; no introduce otro serializer ni carga GPU desde parser |
| editor | Viewport, herramientas, inspector y recorrido integrado | Usa documento nativo y esquemas; no posee una segunda verdad del nivel |

Con dos agentes: integrador/runtime + GPU primero; luego redistribuir contenido/editor. Con tres: integrador, GPU/editor y runtime/contenido. Con cuatro: integrador más tres workers; reasignar rol cuando un ticket termine. No crear agentes ociosos ni superar la concurrencia realmente disponible.

El coordinador reclama ticket y locks antes de delegar, indica base commit, rutas exactas de escritura y contrato vigente. Los workers no modifican backlog/STATE ni integran otros tickets. Devuelven commits o diff, pruebas, riesgos e instrucciones de integración. El coordinador libera locks sólo al integrar o devolver la tarea.

`paths` en backlog son puntos de entrada y áreas permitidas a concretar, incluyendo archivos para lectura; no autorizan editar indiscriminadamente todos los archivos de un directorio. Antes de empezar, cada worker declara su lista de escritura. Cualquier solapamiento real bloquea simultaneidad aunque falte en los locks iniciales. F00 sólo escribe evidencia/estado salvo reparación adicional explícitamente acotada.

## Aislamiento y archivos compartidos

- Preferir worktrees/checkouts aislados por worker, desde un commit integrado que contenga sus dependencias. No usar reset/clean/restore global ni mover trabajo ajeno para preparar una rama.
- Cada checkout usa sus propios `build/retroforge/` y `build/vestigio/`, además de sus `bin/obj/` y artefactos. `tools/build.ps1` acepta `-Engine RetroForge` o `-Engine Vestigio`; sus árboles de build son distintos. Los presets CMake `retroforge-*` y `vestigio-*` también tienen binaryDir separados bajo `build/workspace/`. Un worktree nuevo no trae `.tools`, `.deps` ni `.nuget` ignorados. Compruébelo antes de lanzar builds; no suponga que bootstrap ya ocurrió.
- Reutilizar herramientas/cachés sólo si el coordinador establece rutas y acceso seguro; nunca compartir salidas de compilación. Si no es posible preparar entornos aislados, usar un único escritor y serializar implementación en el checkout disponible.
- `tools/bootstrap.ps1` actualiza paquetes locales además de restaurarlos. No ejecutarlo concurrentemente sobre una caché compartida ni sólo por rutina; F00 decide si hace falta y registra versiones.
- Cambios en el dispatcher raíz `CMakeLists.txt`, presets, `engines/vestigio/CMakeLists.txt`, `engines/vestigio/include/vestigio/`, esquema común y bridges de sesión se reclaman con locks. Un worker puede preparar un cambio en su rama, pero el coordinador revisa e integra de uno en uno. No hay export masivo de símbolos nuevos para evitar diseñar la API.
- Las pruebas que escriben en proyectos existentes trabajan sobre copias aisladas del corpus. No sobrescribir Haunted original, partidas del usuario ni evidencia de otro ticket.

## Estados y entrega por ticket

`PLANNED` → `IN_PROGRESS` → `IMPLEMENTED` → `VERIFIED` → `INTEGRATED`. `BLOCKED` exige motivo y condición concreta de desbloqueo. Elegibilidad se calcula: todas las dependencias INTEGRATED, ticket dentro del encargo, locks libres y writer set disjunto.

IMPLEMENTED indica código escrito; VERIFIED indica aceptación del ticket comprobada en su rama; INTEGRATED exige revisión, incorporación al candidato y checks afectados en ese candidato. Un screenshot, header compilable o test sin comportamiento no permite saltar estados. Si un entorno no permite una comprobación requerida, registrar `NOT_RUN` y no marcar cumplimiento por inferencia.

Entrega mínima del worker: ID, base/result commit, archivos, cambio/razón, comandos y salidas, casos de aceptación, artefactos, límites y siguiente dependencia. Usar [el formato de handoff](AGENT-PROMPTS.md#entrega-de-un-worker). Mantener evidencia por ticket/commit y referencia en STATE; nunca copiar resultados de una rama como si provinieran de otra.

## Decisiones y bloqueos

Una decisión local dentro del contrato corresponde al agente; no necesita pedir permiso por cada función o test. Cambios de frontera se resuelven con nota breve: problema reproducido, dos alternativas relevantes, coste, elección, tickets afectados y transición técnica si hace falta. Actualizar contrato y DAG antes de continuar dependencias.

G02 es el principal riesgo inicial: ventana externa demuestra GPU, pero no el editor embebido. Si falla, conservar experimento/evidencia, bloquear E01 y continuar lo independiente autorizado. La solución no puede ser readback continuo escondido ni reescribir todo Studio sin evaluar coste.

No implementar por anticipación networking, ECS universal, editor de shaders por nodos, game DLL/hot reload, scripting, bindings múltiples, Vulkan/D3D12 propios, PBR completo o MCP. Todo queda fuera de estos tickets salvo instrucción posterior. La licencia de código propio pendiente no bloquea investigación/implementación interna ordinaria; sí impide declarar autorizada una publicación pública bajo una licencia inventada.

## Fin del encargo

Al concluir la oleada asignada, ejecutar su gate dirigido, revisar el diff, integrar y hacer commit. Comprobar `origin/main` antes de hacer push directo sin forzar; si avanzó o diverge, inspeccionar y resolver sin sobrescribir trabajo ajeno. Entregar SHA publicado, evidencia, instrucciones para probar y límites. Las oleadas 4–7 ya tienen autorización consecutiva; después de la 7 se espera revisión y nuevo encargo. Un fallo que el usuario encuentre durante la revisión pertenece a la misma oleada. El push del repositorio está autorizado; este plan no autoriza desplegar el producto ni enviar mensajes a terceros.
