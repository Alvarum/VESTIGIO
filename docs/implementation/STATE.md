# Estado de implementación

Fecha de preparación del plan: 2026-09-20.

- Estado global: **IMPLEMENTACIÓN_EN_CURSO — F00/F01/G01/G02/R01 INTEGRADOS / R02 EN CURSO**.
- HEAD observado para redactar: `28dafa949ff68ed3dc52bf93d287863037bf8578`.
- Checkout observado limpio antes de crear `docs/implementation/`.
- Cambios de código desde `ae48d31` hasta esa base: ninguno; se incorporó la investigación.
- F00 está INTEGRATED en `fe43b83`; F01 en `2052e5b`; G01 en `a13fb8b`; G02 finaliza en `9811b03`; R01 en `9744dae`. R02 implementa el registro de assets sobre ese runtime.
- Encargo de inicio recomendado: **ARRANQUE**. Los prompts permiten seleccionar COMPLETO de forma explícita.
- GPU objetivo: backend principal confirmado por el usuario; G01 y G02 ya ejecutan OpenGL 3.3 real en una NVIDIA GeForce RTX 5060 Ti.
- Matriz actual: analyze 7/7, UBSan 7/7, debug 11/11 y release 11/11; Studio/.NET sin advertencias ni errores.

Verificación del paquete: **PASS**, 31 tickets, 61 dependencias, 15 oleadas teóricas, sin ciclos; todos los tickets alimentan el gate final Z01. Los ocho tickets de ARRANQUE incluyen sus dependencias. TASKS/WAVES coinciden con el JSON. Se comprobaron 20 enlaces locales y seis casos negativos del validador (ciclo, dependencia desconocida, ID duplicado, inicio prematuro, alcance incompleto y gate final incompleto), todos rechazados correctamente. Esto no es una ejecución de los tickets ni de pruebas del motor.

## Registro del coordinador

F00/F01/G01/G02/R01 integrados. R02 mantiene asset-core/public-api.

| Ticket | Responsable / checkout | Base y resultado | Locks / archivos compartidos | Evidencia / siguiente paso |
|---|---|---|---|---|
| F00 | integrador / checkout principal | `28dafa9` / `fe43b83` | liberado | [evidence/F00.md](evidence/F00.md); PASS baseline |
| F01 | integrador / checkout principal | `fe43b83` / `2052e5b` | liberado | [evidence/F01.md](evidence/F01.md); matriz completa PASS |
| G01 | integrador / checkout principal | `2052e5b` / `a13fb8b` | liberado | [evidence/G01.md](evidence/G01.md); GPU real y matriz completa PASS |
| G02 | integrador / checkout principal | `a13fb8b` / `9811b03` | liberado | [evidence/G02.md](evidence/G02.md); HwndHost GPU, 50 ciclos y revisión independiente PASS |
| R01 | agente `r01_runtime` + integrador / checkout compartido | `9811b03` / `9744dae` | liberado | [evidence/R01.md](evidence/R01.md); handles/TRS/iteración y matriz completa PASS |
| R02 | agente `r02_design` / checkout compartido, rutas acotadas | `9744dae` / en curso | asset-core, public-api | leases/residentes/pipeline CPU-GPU y pruebas en curso |

## Bloqueos y decisiones pendientes

No hay un bloqueo que impida comenzar F00. Hay riesgos que deben comprobarse durante implementación:

- G02: docking flotante, Tab/Escape y DPI físico 150/200 % conservan aceptación humana pendiente; el embedding base está integrado.
- Toolchain/cachés en checkouts de agentes: los directorios ignorados no se copian al crear worktree.
- Licencia de código propio del SDK: decisión pendiente para publicación; no elegirla unilateralmente.
- Hardware de rendimiento y aceptación visual/humana: registrar entorno real, no prometer cobertura no disponible.

## Formato de actualización

Por checkpoint indicar commit integrado, tickets INTEGRATED, ticket(s) en curso, locks reales, pruebas y evidencia por commit, bloqueos y candidatos elegibles dentro del encargo. El backlog es la autoridad de estados; TASKS/WAVES se regeneran con `validate-plan.py --render` y se valida antes de entregar.

No actualizar memorias del usuario como parte del seguimiento. El estado de este proyecto vive en estos archivos.
