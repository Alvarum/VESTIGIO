# RetroForge

RetroForge es el motor retro 2.5D del repositorio. Su código nativo, Studio,
pruebas y recursos pertenecen a esta carpeta. Las herramientas comunes de
preparación y los scripts estables siguen en `../../tools/`.

Si recibiste `RetroForge-Windows.zip`, abre `retro_studio.exe` para editar o
`retro_fps.exe` para jugar el ejemplo. `retro_player.exe` reproduce proyectos
exportados. Los archivos del motor 3D VESTIGIO no forman parte de este paquete.

Desde la raíz del repositorio:

```powershell
./tools/build.ps1 -Engine RetroForge -Preset debug -Test
./build/retroforge/debug/bin/retro_studio.exe
```

La guía de uso y la ruta de aprendizaje están en `../../docs/09-retroforge-studio.md`
y `../../docs/00-empezar.md`.
