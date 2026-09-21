# Estado de implementación

Fecha de preparación del plan: 2026-09-20.

- Estado global: **IMPLEMENTACIÓN_EN_CURSO — F00/F01 INTEGRADOS / G01 VERIFICADO**.
- HEAD observado para redactar: `28dafa949ff68ed3dc52bf93d287863037bf8578`.
- Checkout observado limpio antes de crear `docs/implementation/`.
- Cambios de código desde `ae48d31` hasta esa base: ninguno; se incorporó la investigación.
- F00 está INTEGRATED en `fe43b83`; F01 en `2052e5b`. G01 implementó y verificó el primer frame de geometría GPU; espera commit integrado. R01 está elegible pero todavía no iniciado para mantener un único escritor.
- Encargo de inicio recomendado: **ARRANQUE**. Los prompts permiten seleccionar COMPLETO de forma explícita.
- GPU objetivo: backend principal confirmado por el usuario; G01/G02 pendientes de implementación y evidencia.
- Builds/pruebas del engine durante esta planificación: **no ejecutados**. La verificación de este paquete sólo valida documentos y DAG.

Verificación del paquete: **PASS**, 31 tickets, 61 dependencias, 15 oleadas teóricas, sin ciclos; todos los tickets alimentan el gate final Z01. Los ocho tickets de ARRANQUE incluyen sus dependencias. TASKS/WAVES coinciden con el JSON. Se comprobaron 20 enlaces locales y seis casos negativos del validador (ciclo, dependencia desconocida, ID duplicado, inicio prematuro, alcance incompleto y gate final incompleto), todos rechazados correctamente. Esto no es una ejecución de los tickets ni de pruebas del motor.

## Registro del coordinador

F00/F01 integrados. G01 verificado por el integrador; los locks gpu-backend/platform-window se liberan al registrar el commit.

| Ticket | Responsable / checkout | Base y resultado | Locks / archivos compartidos | Evidencia / siguiente paso |
|---|---|---|---|---|
| F00 | integrador / checkout principal | `28dafa9` / `fe43b83` | liberado | [evidence/F00.md](evidence/F00.md); PASS baseline |
| F01 | integrador / checkout principal | `fe43b83` / `2052e5b` | liberado | [evidence/F01.md](evidence/F01.md); matriz completa PASS |
| G01 | integrador / checkout principal | `2052e5b` / commit pendiente | gpu-backend, platform-window | [evidence/G01.md](evidence/G01.md); GPU real y matriz completa PASS |

## Bloqueos y decisiones pendientes

No hay un bloqueo que impida comenzar F00. Hay riesgos que deben comprobarse durante implementación:

- G02: superficie GPU/WPF y contexto/foco/DPI; ventana externa por sí sola no completa embedding.
- Toolchain/cachés en checkouts de agentes: los directorios ignorados no se copian al crear worktree.
- Licencia de código propio del SDK: decisión pendiente para publicación; no elegirla unilateralmente.
- Hardware de rendimiento y aceptación visual/humana: registrar entorno real, no prometer cobertura no disponible.

## Formato de actualización

Por checkpoint indicar commit integrado, tickets INTEGRATED, ticket(s) en curso, locks reales, pruebas y evidencia por commit, bloqueos y candidatos elegibles dentro del encargo. El backlog es la autoridad de estados; TASKS/WAVES se regeneran con `validate-plan.py --render` y se valida antes de entregar.

No actualizar memorias del usuario como parte del seguimiento. El estado de este proyecto vive en estos archivos.
