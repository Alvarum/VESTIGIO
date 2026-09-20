# 09 · RetroForge Studio

## Propósito

RetroForge Studio es el editor de escritorio del motor. Está escrito con .NET
10 y WPF; el mapa, las reglas y el renderizador siguen viviendo en C23. La
interfaz nunca modifica directamente las estructuras internas del motor: usa
la biblioteca `retro_editor`, una frontera C con handles opacos, tamaños
explícitos y errores estructurados.

```text
Studio WPF -> retro_editor.dll -> retro_gameplay -> retro_core
                                      |
                                      +-> proyecto, mapa, reglas y diálogos
```

Esta división importa por dos motivos: Player y Studio interpretan los mismos
archivos, y una asignación creada en C siempre se libera en C.

## Inicio

Después de compilar el preset `debug`, abre:

```powershell
build\debug\bin\retro_studio.exe
```

Sin argumentos carga el Haunted copiado junto al ejecutable. También acepta un
manifiesto directo o la forma explícita:

```powershell
build\debug\bin\retro_studio.exe --project "C:\Mis juegos\Casa\project.retro"
```

Studio crea un bloqueo local por proyecto. Un segundo proceso puede abrir otro
proyecto, pero no puede obtener un segundo historial editable para el mismo
manifiesto.

## Estructura de la ventana

- **Barra superior:** guardar, deshacer, rehacer, validar, ayuda, probar y
  detener.
- **Escena:** árbol buscable de habitaciones, instancias, barreras, triggers,
  luces, definiciones, reglas y conversaciones.
- **Documento central:** espacios Construir, Personajes, Lógica, Diálogos y
  Probar.
- **Inspector:** propiedades editables y conexiones navegables del elemento
  seleccionado.
- **Panel inferior:** recursos utilizados y problemas de validación.

Los paneles AvalonDock se pueden redimensionar, agrupar y desacoplar a otro
monitor. El layout se guarda en `%LOCALAPPDATA%\RetroForge\Studio`. Si un layout
queda incómodo, usa **Ver > Restablecer espacio de trabajo**.

## Espacios de trabajo

### Construir

La planta dibuja la geometría real leída por el motor. La rueda cambia el zoom,
el botón central desplaza y un clic selecciona. Los overlays de zonas y luces
se activan por separado para evitar que oculten el mapa. El inspector permite
editar cotas, alturas, luz y materiales mediante unidades visibles.

La biblioteca inferior separa las piezas colocables de los archivos del
proyecto. Arrastra un personaje, botiquín, llave o interruptor y suéltalo dentro
de una habitación para crear una instancia. Haz clic para seleccionarla,
arrástrala para moverla en una cuadrícula de 25 cm, pulsa Ctrl+D para duplicar o
Supr para eliminar. Studio impide colocar fuera del nivel y explica por qué no
puede borrar una entidad que todavía usan las reglas.

Las herramientas de creación directa de polígonos, puertas, ventanas y
escaleras se incorporarán sobre la misma API de comandos; Studio no muestra
botones para operaciones que todavía no ejecuta.

### Personajes

Cada tarjeta representa una **definición compartida**, no una instancia del
nivel. Al seleccionarla aparecen sus clips de animación y sus fases de jefe.
El inspector edita atlas, tamaño de celda, movimiento, percepción, vida, daño,
invulnerabilidad y consecuencia de captura. En **Conexiones** se enumeran las
instancias que usan esa definición.

Al seleccionar una instancia de personaje aparecen acciones rápidas. Por
ejemplo, **Al morir: llave** crea una regla one-shot completa con el personaje
como origen y el pickup como consecuencia; no hay que copiar identificadores.
El bloque **Al interactuar** permite elegir una conversación o barrera, o
escribir un mensaje corto. **Crear** añade el evento, origen y acción como una
sola operación que también se puede deshacer.

Los clips muestran número de fotogramas, direcciones y repetición. Cada
fotograma conserva celda, duración y evento; el atlas completo no se usa como
un único sprite durante la partida.

### Lógica

Las reglas se presentan como **CUANDO / SI / HACER**. Seleccionar una tarjeta
expande su evento, origen, condiciones tipadas y acciones reales. El inspector
edita prioridad, cooldown y ejecución única. Las conexiones permiten saltar
entre una regla, su origen y los destinos que reciben acciones.

### Diálogos

La columna izquierda contiene los nodos. La vista previa muestra hablante,
texto, opciones, condiciones y destinos. El inspector modifica el nodo
seleccionado. Los destinos inexistentes se consideran un error de proyecto y
deben corregirse antes de exportar.

### Probar

**Probar** guarda primero si hay cambios y abre `retro_player.exe` con el mismo
manifiesto. Si el guardado falla, Player no se inicia. **Detener** cierra
solamente el proceso iniciado por esa sesión de Studio; cerrar Player también
reactiva automáticamente la edición.

La simulación de Player no escribe posiciones, enemigos o puertas de vuelta en
el documento. Guardar una partida y guardar el proyecto son operaciones
distintas.

## Historial y guardado

Toda edición del inspector entra por `re_editor_set_property`. Antes de aplicar
el cambio, C valida el valor y conserva un estado para deshacer. Una operación
inválida no cambia la revisión ni el historial. Deshacer y rehacer actualizan
el árbol, el mapa, el inspector y las conexiones desde la nueva revisión.

El guardado explícito escribe los formatos del proyecto mediante las rutinas de
`retro_gameplay`. El autosave transaccional completo y las herramientas visuales
de creación siguen pendientes; por eso esta documentación no promete aún
recuperación automática de cambios no guardados.

## Archivos del editor

| Archivo | Responsabilidad |
|---|---|
| `include/retro/editor.h` | Contrato C estable consumido por .NET. |
| `src/editor/editor.c` | Documento autorizado, consultas, validación e historial. |
| `src/studio/App.xaml.cs` | Inicio, resolución y bloqueo del proyecto. |
| `src/studio/MainWindow.xaml` | Composición visual y espacios de trabajo. |
| `src/studio/ViewModels/StudioViewModel.cs` | Selección, comandos, conexiones y Player. |
| `src/studio/Models/EditorDocument.cs` | Dueño administrado del handle nativo. |
| `src/studio/Native/EditorNative.cs` | Firmas de interoperabilidad C/C#. |
| `src/studio/Themes/Graphite.xaml` | Colores, tipografía, foco y controles. |

## Exportación

El empaquetado sigue disponible por línea de comandos mientras se completa su
asistente visual:

```powershell
powershell -ExecutionPolicy Bypass -File tools/export-project.ps1 `
  -Project assets/studio/haunted.retro -Output dist/Games
```

El resultado incluye Player, proyecto, recursos necesarios y licencias. Las
rutas se resuelven desde el manifiesto, por lo que una carpeta con espacios no
debe cambiar el funcionamiento.
