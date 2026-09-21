# Estado de implementación

Fecha de preparación del plan: 2026-09-20.

- Estado global: **IMPLEMENTACIÓN_EN_CURSO — F00/F01/G01/G02/R01/R02/R03/D01/M01 INTEGRADOS / I01, G03 Y D02 EN CURSO**.
- HEAD observado para redactar: `28dafa949ff68ed3dc52bf93d287863037bf8578`.
- Checkout observado limpio antes de crear `docs/implementation/`.
- Cambios de código desde `ae48d31` hasta esa base: ninguno; se incorporó la investigación.
- F00 está INTEGRATED en `fe43b83`; F01 en `2052e5b`; G01 en `a13fb8b`; G02 en `9811b03`. R01 fue auditado, corregido y revalidado junto con R02 en `9ed4d5e`.
- Encargo de inicio recomendado: **ARRANQUE**. Los prompts permiten seleccionar COMPLETO de forma explícita.
- GPU objetivo: backend principal confirmado por el usuario; G01 y G02 ya ejecutan OpenGL 3.3 real en una NVIDIA GeForce RTX 5060 Ti.
- Matriz aislada R01/R02: analyze 8/8, UBSan 8/8, debug 8/8 y release 8/8 con apps desactivadas. La evidencia GPU integrada anterior permanece en G01/G02; los cambios R01/R02 no modifican ese backend.

Verificación del paquete: **PASS**, 31 tickets, 61 dependencias, 15 oleadas teóricas, sin ciclos; todos los tickets alimentan el gate final Z01. Los ocho tickets de ARRANQUE incluyen sus dependencias. TASKS/WAVES coinciden con el JSON. Se comprobaron 20 enlaces locales y seis casos negativos del validador (ciclo, dependencia desconocida, ID duplicado, inicio prematuro, alcance incompleto y gate final incompleto), todos rechazados correctamente. Esto no es una ejecución de los tickets ni de pruebas del motor.

## Registro del coordinador

F00/F01/G01/G02/R01/R02/R03/D01/M01 integrados. I01, G03 y D02 avanzan en paralelo con archivos y locks separados.

| Ticket | Responsable / checkout | Base y resultado | Locks / archivos compartidos | Evidencia / siguiente paso |
|---|---|---|---|---|
| F00 | integrador / checkout principal | `28dafa9` / `fe43b83` | liberado | [evidence/F00.md](evidence/F00.md); PASS baseline |
| F01 | integrador / checkout principal | `fe43b83` / `2052e5b` | liberado | [evidence/F01.md](evidence/F01.md); matriz completa PASS |
| G01 | integrador / checkout principal | `2052e5b` / `a13fb8b` | liberado | [evidence/G01.md](evidence/G01.md); GPU real y matriz completa PASS |
| G02 | integrador / checkout principal | `a13fb8b` / `9811b03` | liberado | [evidence/G02.md](evidence/G02.md); HwndHost GPU, 50 ciclos y revisión independiente PASS |
| R01 | agente `r01_runtime` + `r02_design` + integrador | `9811b03` / `9ed4d5e` | liberado | [evidence/R01.md](evidence/R01.md); auditoría corregida y regresiones PASS |
| R02 | agente `r02_design` + integrador | `9744dae` / `9ed4d5e` | liberado | [evidence/R02.md](evidence/R02.md); leases, candidatos CPU/GPU y ownership PASS |
| R03 | agente `r02_design` + integrador | `df05608` / 11 de 11 por perfil | liberado | lifecycle, cámara y SDK instalable |
| I01 | agente `r02_design` / checkout compartido | `656fda7` / en curso | input-settings, public-api | acciones y settings comunes |
| D01 | integrado | `1acbd03` | libre | formato/validadores y corpus CPU |
| M01 | agente `r01_review` + integrador | `656fda7` / 12 de 12 por perfil | liberado | importer glTF/GLB e IR propia |
| G03 | agente `r01_review` / checkout compartido | `656fda7` / en curso | gpu-backend, asset-gpu | renderables/modelos GPU compartidos |
| D02 | agente `r01_runtime` / checkout compartido | `1acbd03` / en curso | document-core, tool-api | documento/transacciones y Tool API |

## Bloqueos y decisiones pendientes

No hay un bloqueo que impida comenzar F00. Hay riesgos que deben comprobarse durante implementación:

- G02: docking flotante, Tab/Escape y DPI físico 150/200 % conservan aceptación humana pendiente; el embedding base está integrado.
- Toolchain/cachés en checkouts de agentes: los directorios ignorados no se copian al crear worktree.
- Licencia de código propio del SDK: decisión pendiente para publicación; no elegirla unilateralmente.
- Hardware de rendimiento y aceptación visual/humana: registrar entorno real, no prometer cobertura no disponible.

## Formato de actualización

Por checkpoint indicar commit integrado, tickets INTEGRATED, ticket(s) en curso, locks reales, pruebas y evidencia por commit, bloqueos y candidatos elegibles dentro del encargo. El backlog es la autoridad de estados; TASKS/WAVES se regeneran con `validate-plan.py --render` y se valida antes de entregar.

No actualizar memorias del usuario como parte del seguimiento. El estado de este proyecto vive en estos archivos.
