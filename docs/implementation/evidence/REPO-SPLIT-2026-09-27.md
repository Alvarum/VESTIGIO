# Separación de repositorios — 2026-09-27

VESTIGIO 3D permanece en `Alvarum/VESTIGIO`; RetroForge 2.5D se extrajo al
repositorio **privado** `Alvarum/RetroForge` y al checkout local
`C:\Users\alvar\Documents\dev\RetroForge`. La extracción parte del commit
VESTIGIO `3f8aa2acfc3bf24e2ee87d818078a33169e7cf27` y crea el commit
RetroForge `61f8988a2f972fd6f664700edf4b2a94cec23169` (127 archivos).
No se reescribió el historial publicado de VESTIGIO: sus referencias 2.5D
anteriores al corte son históricas.

El checkout VESTIGIO deja sólo `engines/vestigio`, documentación 3D y scripts
de compilación/lanzamiento 3D. El checkout RetroForge incluye
`engines/retroforge`, documentación y recursos 2.5D, scripts propios y copia
independiente de `common/`. No hay enlace de código entre ambos repositorios.

Verificación ejecutada tras la separación:

| Comprobación | Resultado |
|---|---|
| RetroForge Debug con `tools/build.ps1 -Preset debug -Test` | 4/4 PASS; Studio sin advertencias ni errores |
| RetroForge Release con `tools/build.ps1 -Preset release -Test -Package` | PASS; ZIP contiene `retro_player.exe` y `retro_studio.exe`, sin VESTIGIO |
| VESTIGIO Debug con `tools/build.ps1 -Preset debug -Test` | 45/45 PASS, incluidas pruebas GPU, Player y Studio; Studio sin advertencias ni errores |
| `python docs/implementation/validate-plan.py --render` | PASS: 90 tickets, 18 oleadas técnicas, 212 dependencias; no ejecuta el motor |

La revisión humana ya rechazó la UX actual de VESTIGIO Studio. Esta separación
no constituye aprobación visual ni inicia un nuevo ticket de interfaz.
