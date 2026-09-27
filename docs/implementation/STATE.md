# Estado de implementación

Fecha de preparación del plan: 2026-09-20.

- Estado global: **16/30 INTEGRADOS; oleadas 4–7 funcionales verificadas, tickets E02/E03/S03/V01/V02/A01 formales incompletos**. La oleada 0 se publicó en `033f650`; J01 está integrado en `7efc54c` y S01/S02 en `22e937a`, ambos revisados y aceptados por el usuario. E01 se implementó en `73fe637`; la edición hobby de la oleada 4 en `53ea8ca`; la puerta de la oleada 5 en `8630e84`; los perfiles visuales de la oleada 6 en `ef3bfb3`. La oleada 7 añade ambiente y puerta audibles, con evidencia en [W07.md](evidence/W07.md). G04 y D03 se retiraron por requerir compatibilidad/migración que este motor nuevo no necesita.
- HEAD observado para redactar: `28dafa949ff68ed3dc52bf93d287863037bf8578`.
- Checkout observado limpio antes de crear `docs/implementation/`.
- Cambios de código desde `ae48d31` hasta esa base: ninguno; se incorporó la investigación.
- F00 está INTEGRATED en `fe43b83`; F01 en `2052e5b`; G01 en `a13fb8b`; G02 en `9811b03`. R01 fue auditado, corregido y revalidado junto con R02 en `9ed4d5e`.
- Oleada 4: **edición hobby sobre E02/E03**, añadir/transformar un pilar y guardar/reabrir/probar desde Studio. Pruebas dirigidas y GPU real verificadas; la revisión manual del usuario queda pendiente. El usuario encargó continuar consecutivamente hasta la oleada 7, cada una con pruebas, evidencia, commit y push separados. Sin autorización para ejecutar más allá de la 7.
- Oleada 5: **interacción/puerta hobby sobre S03**, abierta/cerrada con E en Player y Probar, panel/collider coherentes, cierre bloqueado por jugador y definición persistida. Debug y Release 10/10 pruebas dirigidas, GPU real con capturas; recorrido manual del usuario pendiente. S03 sigue `IN_PROGRESS` por llaves, triggers, auto-cierre, audio y estado mutable restantes.
- Oleada 6: **visual hobby sobre V01/V02**, dos luces y niebla lineal del documento, perfiles Limpio/Retro y preferencia persistente entre Player y Studio. Debug y Release 15/15 pruebas dirigidas, UBSan 4/4, GPU real con capturas; juicio visual del usuario pendiente. V01 sigue `IN_PROGRESS`; V02 `PLANNED` por dependencia formal.
- Oleada 7: **audio hobby sobre A01**, ambiente MusicStream y efecto de puerta en Player/Studio Probar, cuatro buses persistentes, foco/pausa, ruta Unicode y diagnóstico sin dispositivo. Debug y Release 21/21 dirigidas, UBSan 2/2, WASAPI real; escucha humana de dos loops pendiente. A01 sigue `IN_PROGRESS` por catálogo, esquema de emisores, música larga desde disco y aceptación auditiva general. Tras publicar esta oleada, esperar revisión/nuevo encargo.
- GPU objetivo: backend principal confirmado por el usuario; G01 y G02 ya ejecutan OpenGL 3.3 real en una NVIDIA GeForce RTX 5060 Ti.
- Matriz aislada R01/R02: analyze 8/8, UBSan 8/8, debug 8/8 y release 8/8 con apps desactivadas. La evidencia GPU integrada anterior permanece en G01/G02; los cambios R01/R02 no modifican ese backend.
- Matriz I01 aislada: UBSan 14/14; Analyze build y casos 1–13 PASS, con `retro_contracts` PASS tras limpiar un artefacto de ejecución concurrente; Debug/Release nativos 17/17; builds WPF Debug/Release sin warnings ni errores; `retro_studio_authoring` exacto PASS en 466,63 s (CTest dirigido 466,74 s). El timeout inicial de 180 s era insuficiente para recrear 50 contextos; la evidencia oficial es la corrida posterior sin trazas.
- Candidato G03 aislado desde el índice: build Debug 119/119 y CTest dirigido 7/7 PASS; GPU real en NVIDIA GeForce RTX 5060 Ti/OpenGL 3.3, escena World→assets→GPU, aislamiento renderer/context, detach/reattach con un solo reupload y consumidor C11 instalado desde prefijo temporal.

Verificación del paquete actualizado: **PASS**, 30 tickets, 60 dependencias y 15 oleadas teóricas; sin ciclos y todos los tickets alcanzan el gate Z01. Los ocho tickets de ARRANQUE incluyen sus dependencias. TASKS/WAVES coinciden con el JSON. Esto valida el plan, **no** ejecuta tickets ni pruebas del motor.

## Registro del coordinador

F00/F01/G01/G02/G03/R01/R02/R03/I01/D01/M01/D02/J01/S01/S02/E01 integrados. E02, S03, V01 y A01 están `IN_PROGRESS`; E03 y V02 siguen `PLANNED` por dependencia formal, aunque las oleadas 4 y 6 aportan partes funcionales de ellos. A01 partió de un núcleo validado como checkpoint parcial (`8dfce4a`) y la oleada 7 lo conectó a un dispositivo real para el Atrium; aún no equivale al ticket completo. El trabajo parcial S01 de `939dd9c` fue revisado y completado en `22e937a`. J01 fue un hito sin física; S01/S02 añadieron la física acotada en una oleada posterior.

| Ticket | Responsable / checkout | Base y resultado | Locks / archivos compartidos | Evidencia / siguiente paso |
|---|---|---|---|---|
| F00 | integrador / checkout principal | `28dafa9` / `fe43b83` | liberado | [evidence/F00.md](evidence/F00.md); PASS baseline |
| F01 | integrador / checkout principal | `fe43b83` / `2052e5b` | liberado | [evidence/F01.md](evidence/F01.md); matriz completa PASS |
| G01 | integrador / checkout principal | `2052e5b` / `a13fb8b` | liberado | [evidence/G01.md](evidence/G01.md); GPU real y matriz completa PASS |
| G02 | integrador / checkout principal | `a13fb8b` / `9811b03` | liberado | [evidence/G02.md](evidence/G02.md); HwndHost GPU, 50 ciclos y revisión independiente PASS |
| R01 | agente `r01_runtime` + `r02_design` + integrador | `9811b03` / `9ed4d5e` | liberado | [evidence/R01.md](evidence/R01.md); auditoría corregida y regresiones PASS |
| R02 | agente `r02_design` + integrador | `9744dae` / `9ed4d5e` | liberado | [evidence/R02.md](evidence/R02.md); leases, candidatos CPU/GPU y ownership PASS |
| R03 | agente `r02_design` + integrador | `df05608` / 11 de 11 por perfil | liberado | lifecycle, cámara y SDK instalable |
| I01 | agente `r02_design` + integrador | `656fda7` / `683ad2d` | liberado | [evidence/I01.md](evidence/I01.md); matriz nativa y V-WPF PASS |
| D01 | integrado | `1acbd03` | libre | formato/validadores y corpus CPU |
| M01 | agente `r01_review` + integrador | `656fda7` / 12 de 12 por perfil | liberado | importer glTF/GLB e IR propia |
| G03 | agente `r01_review` + revisión `r02_design` + integrador | `656fda7` / `50ece2a` | liberado | [evidence/G03.md](evidence/G03.md); World→GPU, SDK externo y GPU real PASS |
| D02 | agente `r01_runtime` + integrador | `2e15e5e` / 14 de 14 por perfil | liberado | [evidence/D02.md](evidence/D02.md); documento, Tool API e instanciador transaccional |
| J01 | integrador / checkout principal | `7efc54c` / pruebas dirigidas PASS | liberado | [evidence/J01.md](evidence/J01.md); GPU real y controles confirmados por usuario |
| S01 | agente `s01_spatial` + integrador | `939dd9c` / `22e937a` | liberado | [evidence/S01.md](evidence/S01.md); consultas espaciales y colliders verificados |
| S02 | integrador / checkout principal | `e7e9742` / `22e937a` | liberado | [evidence/S02.md](evidence/S02.md); controlador, Player y GPU verificados; aceptado por el usuario |
| E01 | agentes `e01_document`, `e01_studio`, auditoría `e01_audit` + integrador | `f6b0a3e` / `73fe637` | liberado | [evidence/E01.md](evidence/E01.md); Studio GPU y Editar/Probar verificados; revisión humana pendiente |
| E02/E03 parcial (oleada 4) | agentes `wave4_document`, `wave4_host`, `wave4_studio` + integrador | `bdd8ebf` / `53ea8ca` | liberado | [evidence/W04.md](evidence/W04.md); recorrido hobby 6/6 Debug y Release; tickets formales incompletos |
| S03 parcial (oleada 5) | agentes `wave4_document`, `wave4_host`, `wave4_studio` + integrador | base `ffd32bc` / ver historial de oleada 5 | liberado | [evidence/W05.md](evidence/W05.md); puerta hobby 10/10 Debug y Release; ticket formal incompleto |
| V01/V02 parcial (oleada 6) | agentes `wave4_document`, `wave4_host`, `wave4_studio` + integrador | base `8630e84` / ver historial de oleada 6 | liberado | [evidence/W06.md](evidence/W06.md); visual hobby 15/15 Debug y Release; tickets formales incompletos |
| A01 parcial (oleada 7) | agentes `wave4_document`, `wave4_host`, `wave4_studio` + integrador | base `ef3bfb3` / ver historial de oleada 7 | liberado | [evidence/W07.md](evidence/W07.md); audio hobby 21/21 Debug y Release, WASAPI real; ticket formal incompleto |

## Bloqueos y decisiones pendientes

J01 fue revisado por el usuario: pudo moverse, mirar y cerrar con Escape; aceptó su presentación visual básica. S01/S02 fue revisado por el usuario: confirmó que funciona correctamente y encargó continuar. La revisión interactiva de la ventana completa E01 sigue pendiente. Otros riesgos del plan:

- G02: docking flotante, Tab/Escape y DPI físico 150/200 % conservan aceptación humana pendiente; el embedding base está integrado.
- Toolchain/cachés en checkouts de agentes: los directorios ignorados no se copian al crear worktree.
- Licencia de código propio del SDK: decisión pendiente para publicación; no elegirla unilateralmente.
- Hardware de rendimiento y aceptación visual/humana: registrar entorno real, no prometer cobertura no disponible.

## Formato de actualización

Por checkpoint indicar commit integrado, tickets INTEGRATED, ticket(s) en curso, locks reales, pruebas y evidencia por commit, bloqueos y candidatos elegibles dentro del encargo. El backlog es la autoridad de estados; TASKS/WAVES se regeneran con `validate-plan.py --render` y se valida antes de entregar.

No actualizar memorias del usuario como parte del seguimiento. El estado de este proyecto vive en estos archivos.
