# Prompts para entregar el plan

Copiar uno de los prompts siguientes al agente que tenga acceso al repositorio. Por defecto el encargo es ARRANQUE. Para ejecutar el plan entero, cambiar esa línea por `ALCANCE: COMPLETO`. No lanzar todos los prompts de worker a la vez: el coordinador asigna tickets elegibles.

## Un solo agente

```text
Implementa VESTIGIO siguiendo el paquete docs/implementation/README.md del repositorio:
C:\Users\alvar\Documents\dev\doom like\1

ALCANCE: ARRANQUE — F00, F01, G01, G02, R01, R02, R03, I01.

Esto es una solicitud de implementación, no de volver a planificar. Lee PLAN.md,
CONTRACTS.md, VALIDATION.md, STATE.md y los tickets de backlog.json/TASKS.md.
La investigación de docs/research explica las decisiones; no repitas toda su auditoría.

Empieza por F00 y F01. Comprueba HEAD y trabajo concurrente. Después implementa
los tickets del alcance en orden de dependencias. GPU real es requisito: geometría,
materiales y efectos no pueden seguir calculándose sólo en CPU. SDK C y Studio deben
usar el mismo runtime. Preserva herramientas/niveles legacy y el renderer CPU de laboratorio.

No hagas refactors globales, cambios de licencia, un ECS universal ni MCP/game DLL.
Resuelve decisiones locales sin preguntarme por cada paso. Si una frontera falla,
documenta evidencia y decisión; continúa con trabajo independiente autorizado.

Por ticket: implementar, probar casos de aceptación, revisar diff, integrar y registrar
resultados en evidence/<ID>.md y STATE/backlog. No marques PASS lo que no ejecutaste.
No alteres trabajo ajeno ni compartas salidas de build con otro escritor.

Detente al cerrar ARRANQUE o ante un bloqueo real sin trabajo independiente dentro
del alcance. Entrega commits/archivos, pruebas y límites, estado GPU/WPF/SDK y próximo
ticket elegible. No publiques ni despliegues remotamente.
```

## Coordinador de varios agentes

```text
Coordina e implementa VESTIGIO con agentes siguiendo docs/implementation/README.md
en C:\Users\alvar\Documents\dev\doom like\1.

ALCANCE: ARRANQUE — F00, F01, G01, G02, R01, R02, R03, I01.
Puedes delegar subtareas independientes a agentes dentro de la concurrencia disponible.
Mantén un integrador y asigna roles GPU/runtime/content/editor según tickets elegibles;
no es necesario que todos esos roles estén activos simultáneamente.

Ejecuta F00/F01 primero. Tú eres el único escritor de STATE.md/backlog.json y de la
rama integrada. Publica contratos y base commit antes de delegar. Usa checkouts aislados
con herramientas verificadas y build/obj/bin propios. Si no puedes aislar, serializa
escritura en vez de mandar varios agentes al mismo archivo.

Lee PLAN.md y valida DAG/locks. Asigna al worker ID, base, contrato, escritura exacta,
casos de aceptación y formato de handoff. En ARRANQUE los carriles son G01→G02 y
R01→R02→R03→I01; G01 y R01 parten de F01 integrado. Serializa los cambios de CMake,
API pública, plataforma y viewport que se solapen. No delegues sólo 'hacer render'
o 'hacer el editor': delega tickets concretos.

GPU real, mismo runtime para C y Studio, ownership y round-trip son obligatorios.
No aceptes wrappers del framebuffer CPU como renderer GPU ni ventana externa como
editor embebido completado. No repitas investigación ni amplíes a MCP/Vulkan/ECS universal.

Integra de uno en uno, ejecuta pruebas afectadas sobre el candidato y actualiza estados.
Una tarea bloqueada no detiene las independientes. Al completar el alcance ejecuta su
gate integrado y entrega resultados reales, artefactos, límites y siguiente encargo.
No envíes mensajes externos ni publiques/despliegues por el solo hecho de terminar.
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

La GPU es backend principal; conserva ownership/errores y compatibilidad delimitada.
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

INTEGRATED lo registra el coordinador tras incorporar y verificar el candidato. Los prompts individuales no autorizan rebasar el alcance encargado; para continuar a todo el roadmap hay que encargar COMPLETO o los tickets siguientes.
