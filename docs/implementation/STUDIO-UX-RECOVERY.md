# Recuperar VESTIGIO Studio para crear el primer nivel

**Estado: implementación UX01 entregada para revisión, 2026-10-04.**
El usuario autorizó este plan. El recorrido nuevo y el editor reorganizado
se documentan en [evidence/UX01.md](evidence/UX01.md). La aceptación visual e
interactiva permanece pendiente; no se inicia E05 ni H01–H12 automáticamente.

## Diagnóstico previo que motivó UX01

- `App.xaml.cs` abre el Atrium por defecto y sólo acepta otro nivel mediante
  `--level`. La ventana exige un archivo existente; no existe una acción visible
  para empezar un nivel propio o abrir otro desde la aplicación.
- `Vestigio3DWindow.xaml` pone herramientas, jerarquía, constructor de salas,
  assets, transformación, componentes, problemas y audio en una columna fija de
  370 px con un único scroll. La construcción de habitaciones ocupa esa columna
  incluso cuando no es la tarea activa. La barra superior mezcla Probar,
  creación, edición, guardado y perfil gráfico sin jerarquía clara.
- `Save_Click` sólo ofrece guardar una copia del Atrium la primera vez;
  `Reopen_Click` recarga el archivo activo. Hay protección básica ante cambios
  al cerrar o reabrir, pero no un recorrido Nuevo → Abrir → Guardar como.
- E02–E04 ya aportan comandos de jerarquía, transformación, importación y
  recetas de habitación. Sus pruebas prueban operaciones y round-trip, no la
  composición visual ni que alguien pueda descubrir y completar el flujo.

La captura entregada por el usuario y el XAML coinciden en el problema de
densidad y orientación. No se ha realizado una nueva prueba humana de uso ni
se presenta este diagnóstico como una medición de usabilidad.

## Objetivo de UX01

Al abrir Studio, una persona elige **Nuevo nivel**, **Abrir nivel** o **Ver
Atrium de ejemplo**. En un nivel nuevo, dibuja una habitación con abertura,
importa y coloca un GLB, ajusta su posición, guarda, cierra, reabre y pulsa
**Probar**. Al detener, sigue en su documento y puede deshacer una edición.
Eso permite empezar una escena 3D propia sin editar JSON ni copiar el Atrium.

La composición implementada sigue esta distribución:

```text
Archivo  Editar  Ver                         Nivel propio *      [Probar]
────────────────────────────────────────────────────────────────────────
Escena / jerarquía │ Herramientas · viewport GPU 3D │ Inspector contextual
                   │                                 │ selección o nivel
                   │                                 │
───────────────────┴──────── Recursos │ Problemas ─┴────────────────────
Estado: Editar · guardado/pendiente · mensajes accionables
```

La jerarquía queda a la izquierda, el viewport es la superficie principal y
el inspector sólo muestra propiedades de la selección o del nivel. Recursos
y problemas ocupan una zona inferior con pestañas. **Construir habitación**
abre una herramienta enfocada con vista previa y Cancelar/Crear, en vez de
mantener sus campos entre todas las propiedades. Paneles redimensionables y
un estado vacío claro evitan que la primera pantalla parezca una demo rota.
El Atrium sigue como ejemplo accesible, no como documento inicial obligatorio.

## Límite del ticket y cierre

UX01 reutiliza GPU, documento, herramientas E02–E04 y formato actual. Puede
ajustar el ciclo de vida del documento nativo para crear/abrir niveles y
proteger cambios pendientes. No incluye sistema completo de proyectos,
docking libre, exportación, puertas con llave, animación, diálogo, un nuevo
renderer, ni migraciones o compatibilidad con RetroForge.

La aceptación exige un recorrido real y repetible en **un archivo nuevo**:
Nuevo → habitación/abertura → GLB → transformar → Guardar → cerrar → Abrir →
Probar → Detener. Se comprueban Cancelar ante cambios sin guardar, undo/redo,
ausencia de cambios del documento durante Play y errores visibles junto a la
acción que falló. La composición se captura y revisa a 1366×768 y 1920×1080,
DPI 100% y 150%, con controles esenciales visibles y navegación por teclado.
La revisión visual e interactiva del usuario es la puerta final: pruebas WPF,
CTest o una captura aislada no bastan para marcar UX01 `INTEGRATED`.

Se entrega UX01 con evidencia, pasos para probar, commit y push; se corrigen
fallos de esa revisión dentro del mismo ticket. Sólo después se decide si el
siguiente valor es colocación directa en viewport (H01), catálogo de modelos
(H02) o el recorrido editorial avanzado (E05).

## Razón técnica del diseño

La división de paneles se puede implementar con `Grid` y `GridSplitter` de WPF,
que redistribuye espacio entre filas o columnas. El foco de teclado requiere
comprobar alcance y orden de tabulación entre viewport y paneles, no sólo
añadir atajos. Referencias oficiales: [GridSplitter](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/controls/gridsplitter-how-to-topics),
[foco en WPF](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/focus-overview)
y [pruebas de accesibilidad de Windows](https://learn.microsoft.com/en-us/windows/win32/winauto/accessibility-testingtools).
