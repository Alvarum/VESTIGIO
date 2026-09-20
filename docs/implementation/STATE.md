# Estado de implementación

Fecha de preparación del plan: 2026-09-20.

- Estado global: **PLAN_LISTO / IMPLEMENTACIÓN_NO_INICIADA**.
- HEAD observado para redactar: `28dafa949ff68ed3dc52bf93d287863037bf8578`.
- Checkout observado limpio antes de crear `docs/implementation/`.
- Cambios de código desde `ae48d31` hasta esa base: ninguno; se incorporó la investigación.
- Todos los tickets del nuevo backlog están PLANNED. El engine legacy existente no equivale a haber implementado estos tickets nuevos.
- Único ticket inicialmente elegible: **F00**. Al integrarse F00: F01. Después de F01: G01 y R01, si sus escrituras no se solapan.
- Encargo de inicio recomendado: **ARRANQUE**. Los prompts permiten seleccionar COMPLETO de forma explícita.
- GPU objetivo: backend principal confirmado por el usuario; G01/G02 pendientes de implementación y evidencia.
- Builds/pruebas del engine durante esta planificación: **no ejecutados**. La verificación de este paquete sólo valida documentos y DAG.

Verificación del paquete: **PASS**, 31 tickets, 61 dependencias, 15 oleadas teóricas, sin ciclos; todos los tickets alimentan el gate final Z01. Los ocho tickets de ARRANQUE incluyen sus dependencias. TASKS/WAVES coinciden con el JSON. Se comprobaron 20 enlaces locales y seis casos negativos del validador (ciclo, dependencia desconocida, ID duplicado, inicio prematuro, alcance incompleto y gate final incompleto), todos rechazados correctamente. Esto no es una ejecución de los tickets ni de pruebas del motor.

## Registro del coordinador

Sin asignaciones ni locks reclamados todavía. Actualizar esta sección al empezar F00; no sustituir el estado por planes vagos.

| Ticket | Responsable / checkout | Base y resultado | Locks / archivos compartidos | Evidencia / siguiente paso |
|---|---|---|---|---|
| F00 | Sin asignar | Confirmar HEAD al iniciar | integration | Ejecutar baseline y revisar A01–A14 |

## Bloqueos y decisiones pendientes

No hay un bloqueo que impida comenzar F00. Hay riesgos que deben comprobarse durante implementación:

- G02: superficie GPU/WPF y contexto/foco/DPI; ventana externa por sí sola no completa embedding.
- Toolchain/cachés en checkouts de agentes: los directorios ignorados no se copian al crear worktree.
- Licencia de código propio del SDK: decisión pendiente para publicación; no elegirla unilateralmente.
- Hardware de rendimiento y aceptación visual/humana: registrar entorno real, no prometer cobertura no disponible.

## Formato de actualización

Por checkpoint indicar commit integrado, tickets INTEGRATED, ticket(s) en curso, locks reales, pruebas y evidencia por commit, bloqueos y candidatos elegibles dentro del encargo. El backlog es la autoridad de estados; TASKS/WAVES se regeneran con `validate-plan.py --render` y se valida antes de entregar.

No actualizar memorias del usuario como parte del seguimiento. El estado de este proyecto vive en estos archivos.
