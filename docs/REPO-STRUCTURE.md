# Estructura de VESTIGIO

Este repositorio contiene el motor **VESTIGIO 3D**:

| Carpeta | Contenido |
|---|---|
| `engines/vestigio/` | Código nativo, SDK, Studio WPF, pruebas, recursos y `cgltf` |
| `common/` | Configuración y mapeo de entrada usados por VESTIGIO |
| `tools/` | Preparación, compilación, verificación y lanzadores del motor 3D |
| `docs/implementation/` | Tickets y evidencia de VESTIGIO |
| `docs/research/` | Investigación y referencias históricas |

Desde la raíz, `./tools/build.ps1 -Preset debug -Test` construye sólo VESTIGIO
en `build/vestigio/debug/bin/`. `./tools/run-studio-3d.ps1` abre el editor y
`./tools/run-3d-demo.ps1` abre Player. El código 2.5D está en el repositorio
Git local `C:\Users\alvar\Documents\dev\RetroForge`; su remoto privado es
[Alvarum/RetroForge](https://github.com/Alvarum/RetroForge). No se compila ni
se enlaza desde este repositorio.

El historial anterior al traslado conserva el desarrollo de ambos motores;
se mantiene sin reescritura para no alterar commits publicados. Las menciones
a RetroForge en investigaciones antiguas son referencias históricas, no código
activo ni un formato compatible que VESTIGIO deba admitir.
