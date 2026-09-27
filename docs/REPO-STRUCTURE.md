# Dos motores, dos árboles de trabajo

| Carpeta | Propietario | Contenido |
|---|---|---|
| `engines/retroforge/` | RetroForge 2.5D | C nativo, Studio WPF, pruebas y recursos de RetroForge |
| `engines/vestigio/` | VESTIGIO 3D | C nativo, Studio WPF, pruebas, demo, integración GPU, `cgltf` y paquete SDK |
| `common/` | Ambos | Sólo configuración y mapeo de entrada; no contiene escena, renderer, juego ni editor |
| `tools/` | Workspace | Preparación, compilación, comprobación y lanzadores que eligen motor explícitamente |
| `docs/` | Workspace | Guías y el backlog de desarrollo de VESTIGIO |

Cada motor tiene su propio `CMakeLists.txt`, presets, `src/`, `include/`,
`tests/`, `assets/`, `studio/` y `studio.tests/`. El CMake de la raíz sólo
selecciona **uno** con `-DENGINE=retroforge` o `-DENGINE=vestigio`; no enlaza
los motores. `tools/build.ps1` compila directamente el CMake de la carpeta
elegida. Los productos se escriben en `build/retroforge/<preset>/bin/` y
`build/vestigio/<preset>/bin/`, respectivamente.

Desde la raíz, tras `./tools/bootstrap.ps1`:

```powershell
./tools/build.ps1 -Engine RetroForge -Preset debug -Test
./tools/build.ps1 -Engine Vestigio -Preset debug -Test
./build/retroforge/debug/bin/retro_studio.exe
./build/vestigio/debug/bin/vestigio_studio.exe
```

Para abrir el Atrium 3D con sus recursos usa `./tools/run-3d-demo.ps1` o
`./tools/run-studio-3d.ps1`. [La guía de VESTIGIO](../VESTIGIO-COMO-USAR.md)
explica controles y autoría; [la guía de RetroForge](09-retroforge-studio.md)
explica el proyecto 2.5D. Cambiar el árbol de un motor no exige compilar el
otro. No hay migración ni compatibilidad entre formatos de juego.

El código de `common/` conserva el prefijo público `Vg` de las funciones de
configuración existentes para no alterar su contrato durante este traslado.
Esta pequeña biblioteca se compila una vez dentro de **cada** build; no crea
una dependencia binaria de RetroForge hacia VESTIGIO ni viceversa.
