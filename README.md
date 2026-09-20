# RetroForge Studio / Foundry

> Estado actual: base en desarrollo. El formato `retro_map 4` incorpora
> aberturas parciales y evita que conectar dos habitaciones elimine toda la
> pared. Consulta [Portales y recursos visuales](docs/14-portales-y-recursos-visuales.md).

Motor retro en **C23**, renderer propio en CPU y plataforma **raylib 6.0**.
Incluye gameplay dirigido por datos, el editor visual RetroForge Studio, un
reproductor genérico, Foundry y el laboratorio independiente.

## Empezar en Windows

Desde PowerShell, en esta carpeta:

```powershell
./tools/bootstrap.ps1
./tools/build.ps1 -Preset debug -Test
./build/debug/bin/retro_fps.exe
./build/debug/bin/retro_lab.exe
./build/debug/bin/retro_studio.exe
```

Studio abre `assets/studio/haunted.retro`: una casa con plantas superpuestas,
triggers 3D, diálogos, objetivos, luces dinámicas, escalera, perseguidora, drop
de llave y jefe de dos fases. **PROBAR** ejecuta una copia aislada del nivel;
**EXPORTAR** genera una carpeta y ZIP para Windows.

Las herramientas viven en `.tools`; no se modifica el MSYS2 global ni el PATH
permanente. La primera preparación necesita Internet y espacio para herramientas.

Si PowerShell bloquea scripts, usa `powershell -NoProfile -ExecutionPolicy Bypass
-File tools/build.ps1 -Preset debug -Test` (todo en una línea). La opción afecta
sólo a ese proceso. El mismo patrón sirve para `tools/bootstrap.ps1`.

```powershell
./tools/build.ps1 -Preset release -Test -Package
```

El paquete se genera en `dist/RetroForge-Windows.zip`.

## Controles

WASD: moverse; ratón: mirar; clic izquierdo: disparar; Espacio: saltar;
E: interactuar; Escape: pausa; flechas y Enter: menús. F11 alterna ventana sin
bordes. F1: plano, F2: geometría, F3: profundidad, F4: contadores del FPS.
En el reproductor genérico F5 guarda rápido y F9 carga. El menú de pausa permite
usar tres ranuras manuales; los checkpoints crean autoguardado.

**Objetivo:** recoger la llave naranja en el depósito lateral, abrir la compuerta
con E y llegar a la terminal de salida. Hay tres guardias y suministros.

## Estudiar el proyecto

Empieza por [la ruta de aprendizaje](docs/00-empezar.md) y
[el índice de archivos](docs/01-arquitectura.md). La documentación cubre C,
matemáticas, memoria, rasterización, física, mapas, juego y reutilización.

La investigación original permanece en `deep-research-report.md`.
Consulta [verificación y límites](docs/08-verificacion.md) para distinguir las
pruebas ejecutadas de las comprobaciones manuales pendientes.

Continúa con [el tutorial de Studio](docs/09-retroforge-studio.md),
[personajes y perseguidores](docs/10-gameplay-y-personajes.md) y
[los formatos v3](docs/11-formato-v2.md). La capa nueva se estudia en
[lógica e interacciones](docs/12-logica-interacciones.md) y
[diálogos, guardado e iluminación](docs/13-dialogos-guardado-iluminacion.md).
