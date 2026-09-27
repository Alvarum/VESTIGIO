# Separación de RetroForge y VESTIGIO

Fecha: 2026-09-27. Alcance: organización del repositorio y compilación; no añade
funciones al editor 3D ni resuelve el rechazo de su UX.

RetroForge 2.5D ocupa `engines/retroforge/` y VESTIGIO 3D ocupa
`engines/vestigio/`. Cada árbol contiene fuentes C, cabeceras, Studio WPF,
pruebas y recursos propios. El CMake raíz selecciona uno; los scripts
`tools/build.ps1 -Engine RetroForge|Vestigio` generan binarios separados en
`build/<engine>/<preset>/bin/`. El único código C compartido está en `common/`:
configuración y mapeo de entrada. `retro_session` ya no enlaza
`vestigio_runtime`; cada motor compila su propia copia de esa biblioteca común.
El Studio 2.5D se llama `retro_studio.exe` y el 3D `vestigio_studio.exe`; el
interruptor `--atrium` deja de seleccionar un segundo editor dentro del
primero. `cgltf` y el paquete CMake del SDK quedan con VESTIGIO.

Verificación realizada en Windows x64:

| Comprobación | Resultado |
|---|---|
| `tools/build.ps1 -Engine RetroForge -Preset debug -Test` | 4/4 PASS; Studio 0 advertencias/errores |
| `tools/build.ps1 -Engine RetroForge -Preset release -Test` | 4/4 PASS; Studio 0 advertencias/errores |
| `tools/build.ps1 -Engine Vestigio -Preset debug -Test` | 45/45 PASS, incluida GPU, Studio y consumidor SDK instalado; Studio 0 advertencias/errores |
| `tools/build.ps1 -Engine Vestigio -Preset release -Test` | 45/45 PASS; Studio 0 advertencias/errores |
| `tools/build.ps1 -Engine Vestigio -Preset analyze -Test` | 19/19 PASS |
| `tools/build.ps1 -Engine Vestigio -Preset ubsan -Test` | 19/19 PASS |
| `tools/run-3d-demo.ps1 -Smoke 2 -NoAudio` | Arranque real: NVIDIA GeForce RTX 5060 Ti, OpenGL 3.3; `lights=2 fog=linear`; 2 frames, 16 draws, 192 triángulos |
| `tools/build.ps1 -Engine RetroForge -Preset release -Package` | PASS; ZIP incluye `retro_studio.exe` y no contiene VESTIGIO |
| `python docs/implementation/validate-plan.py --render` | PASS; 90 tickets, 18 niveles técnicos, 212 dependencias; rutas activas de tickets 3D sin fuentes RetroForge |

La validación del plan verifica metadatos, no experiencia visual. El usuario
rechazó la usabilidad del Studio 3D mostrado antes de esta separación;
sus pruebas automatizadas no sustituyen esa revisión. El siguiente entregable
debe tratar esa UX explícitamente y quedar sujeto a su revisión.
