# Prompts para entregar el plan

Copiar un prompt sólo cuando el usuario encargue una oleada o ticket de [ENTREGABLES-HOBBY.md](ENTREGABLES-HOBBY.md). ARRANQUE ya está integrado. La instrucción anterior `ALCANCE: COMPLETO` no autoriza continuar automáticamente. El usuario encargó las oleadas 4–9 y después pidió **un ticket completo**; se cerró E02 con todos sus criterios. El siguiente ticket requiere un encargo nuevo. El motor es nuevo y no requiere compatibilidad ni migraciones de proyectos previos. Cada entrega cierra con commit, pruebas y push a `origin/main`.

## Un solo agente

```text
Implementa VESTIGIO siguiendo el paquete docs/implementation/README.md del repositorio:
C:\Users\alvar\Documents\dev\doom like\1

ALCANCE: [oleada que el usuario haya encargado y tickets concretos].
LÍMITE: sólo [esa oleada]. Sin tiempo fijo; informar avances sin ampliar el alcance.

Esto es una solicitud de implementación, no de volver a planificar. Lee PLAN.md,
CONTRACTS.md, VALIDATION.md, STATE.md y los tickets de backlog.json/TASKS.md.
La investigación de docs/research explica las decisiones; no repitas toda su auditoría.

Comprueba HEAD, trabajo concurrente y dependencias integradas de los tickets elegidos.
J01, S01/S02, E01 y E02 ya están integrados; no los repitas. El último encargo autorizó sólo E02 completo. E03 y los demás tickets pendientes no se inician hasta un encargo nuevo.
Implementa únicamente los tickets encargados usando el SDK C y GPU real cuando aplique.
No migres Haunted ni agregues adaptadores o compatibilidad con prototipos anteriores.

No hagas refactors globales, cambios de licencia, un ECS universal ni MCP/game DLL.
Resuelve decisiones locales sin preguntarme por cada paso. Si una frontera falla,
documenta evidencia y decisión; continúa con trabajo independiente autorizado.

Por ticket: implementar, probar casos de aceptación, revisar diff, integrar y registrar
resultados en evidence/<ID>.md y STATE/backlog. No marques PASS lo que no ejecutaste.
No alteres trabajo ajeno ni compartas salidas de build con otro escritor.

Detente al cerrar la oleada encargada o ante un bloqueo real sin trabajo independiente dentro
del alcance. Tras las pruebas y el commit integrado, comprueba el remoto y haz
push directo a origin/main sin forzar. Entrega SHA publicado, archivos, pruebas,
límites, pasos para probar el resultado y siguiente paso posible. No inicies otra oleada.
```

## Coordinador de varios agentes

```text
Coordina e implementa VESTIGIO con agentes siguiendo docs/implementation/README.md
en C:\Users\alvar\Documents\dev\doom like\1.

ALCANCE: [oleada elegida por el usuario y tickets concretos].
LÍMITE: sólo [oleada elegida por el usuario y tickets concretos], sin tiempo fijo.
Informa avances y no amplíes el alcance. Usa un agente por defecto; delega en
paralelo sólo si el usuario lo encargó para esta oleada.

F00/F01 ya están integrados. Tú eres el único escritor de STATE.md/backlog.json y de la
rama integrada. Publica contratos y base commit antes de delegar. Usa checkouts aislados
con herramientas verificadas y build/obj/bin propios. Si no puedes aislar, serializa
escritura en vez de mandar varios agentes al mismo archivo.

Lee PLAN.md y valida DAG/locks. Asigna al worker ID, base, contrato, escritura exacta,
casos de aceptación y formato de handoff. Serializa los cambios de CMake,
API pública, plataforma y viewport que se solapen. No delegues sólo 'hacer render'
o 'hacer el editor': delega tickets concretos.

GPU real, mismo runtime para C y Studio y ownership son obligatorios según el ticket.
No aceptes wrappers del framebuffer CPU como renderer GPU ni ventana externa como
editor embebido completado. No repitas investigación ni amplíes a MCP/Vulkan/ECS universal.

Integra de uno en uno, ejecuta pruebas afectadas sobre el candidato y actualiza estados.
Una tarea bloqueada no detiene las independientes dentro del mismo encargo. Al completar
la oleada, comprueba el remoto y publica los commits integrados en origin/main
sin forzar. Entrega SHA, pruebas, instrucciones de prueba y pendientes; espera
la revisión del usuario antes de iniciar otra oleada. No despliegues el producto.
```

## Asignación a un worker

El coordinador reemplaza todos los campos entre corchetes. Este bloque es una plantilla de delegación, no un comando listo para ejecutar sin asignación.

```text
Implementa únicamente el ticket [ID y título] de docs/implementation/backlog.json.
Checkout: [ruta aislada]. Base integrada: [commit]. Contrato vigente: [versión/commit].
Dependencias integradas: [IDs y commits]. Rol: [rol]. Locks reclamados: [lista].
Puedes escribir sólo: [archivos/directorios concretos, nuevos o existentes].
Archivos compartidos reservados al integrador: [lista].

Lee la ficha TASKS.md, CONTRACTS.md y los perfiles de VALIDATION.md de este ticket.
Implementa el comportamiento y pruebas necesarias, sin completar otros tickets ni
refactorizar fuera de alcance. Puedes leer dependencias, pero no cambiar sus contratos
sin avisar al coordinador. No edites STATE/backlog ni integres ramas de otros workers.

La GPU es backend principal; conserva ownership y manejo de errores. No agregues
compatibilidad con formatos o proyectos anteriores sin un encargo nuevo.
Registra evidencia del ticket en docs/implementation/evidence/[ID].md. Si una condición
no puede ejecutarse, márcala NOT_RUN/BLOCKED_ENV, nunca PASS supuesto.

Entrega base/result commit o diff, cambios, casos verificados, comandos/exit codes,
artefactos y limitaciones. Si estás bloqueado, indica la condición exacta de desbloqueo.
```

## Revisión independiente antes de integrar

```text
Revisa el ticket [ID] sobre base [commit] y candidato [commit]. No modifiques código.
Contrasta diff y pruebas con backlog.json, CONTRACTS.md y VALIDATION.md. Comprueba
scope, ownership, errores, GPU real cuando aplique y consumers afectados. Busca criterios
de aceptación omitidos, stubs o PASS históricos atribuidos al candidato.
Devuelve hallazgos con archivo/símbolo y reproducción, criterios cubiertos/pendientes
y recomendación integrar/devolver. Un build exitoso no basta para aceptar comportamiento.
```

## Entrega de un worker

```text
Ticket / rol:
Base / commit de resultado / dirty state:
Estado propuesto: IMPLEMENTED | VERIFIED | BLOCKED
Archivos cambiados y motivo:
Contratos respetados o cambio solicitado:
Criterios de aceptación: resultado y evidencia de cada uno:
Comandos, exit codes y pruebas realmente ejecutadas:
Artefactos/capturas/logs y rutas accesibles:
Riesgos, límites, NOT_RUN y bloqueos:
Pasos concretos de integración y comprobaciones del integrador:
```

INTEGRATED lo registra el coordinador tras incorporar y verificar todos los criterios del ticket. Los prompts individuales no autorizan rebasar la oleada encargada; el usuario ya encargó las oleadas 4–7 en orden, y cualquier trabajo posterior requiere otra decisión.
