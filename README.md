# VESTIGIO 3D

Motor 3D en desarrollo con render OpenGL 3.3 en GPU, Player y Studio WPF.
Este repositorio contiene sólo VESTIGIO. RetroForge 2.5D tiene un repositorio
Git independiente en `C:\Users\alvar\Documents\dev\RetroForge`, publicado en
el repositorio privado https://github.com/Alvarum/RetroForge.

Desde PowerShell en la raíz del repositorio:

```powershell
./tools/bootstrap.ps1
./tools/build.ps1 -Preset debug -Test
./tools/run-3d-demo.ps1
./tools/run-studio-3d.ps1
```

El código, las pruebas y los recursos 3D están en `engines/vestigio/`; los
binarios se generan en `build/vestigio/debug/bin/`. El módulo `common/` contiene
sólo configuración y mapeo de entrada. [La guía de uso](VESTIGIO-COMO-USAR.md)
detalla los controles, la compilación y el flujo actual de edición.

**Estado del editor:** Player y Studio compilan y tienen pruebas automatizadas,
pero el usuario rechazó la usabilidad de la interfaz 3D mostrada. El
[plan de recuperación de Studio](docs/implementation/STUDIO-UX-RECOVERY.md)
propone UX01 como siguiente ticket revisable antes de ampliar funciones.

La [investigación](docs/research/README.md) y el [backlog](docs/implementation/README.md)
conservan referencias históricas de proyectos estudiados. Esas referencias no
son dependencias de código de RetroForge ni implican compatibilidad entre motores.
