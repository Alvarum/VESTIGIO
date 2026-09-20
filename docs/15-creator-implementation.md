# RetroForge Creator: seguimiento de implementación

Este documento distingue las capacidades verificadas del objetivo completo.
El plan aprobado se entrega por recorridos integrados; una declaración de API
no equivale a una función terminada.

## Orden y condiciones de cierre

1. Base fiable: historial transaccional, sesión compartida, prueba aislada y
   persistencia recuperable.
2. Construcción: habitaciones, conexiones, plantas, escaleras y vista 3D.
3. Contenido y eventos: recursos, animaciones, condiciones, acciones y diálogos.
4. Audio y superficies: música, efectos, voces locales y materiales animados.
5. Combate: armas, proyectiles, invocaciones y jefes operativos.
6. Juegos completos: campañas, plataformas, menús y snapshots completos.
7. Entrega: tutoriales, exportación, pruebas visuales y aceptación humana.

## Contratos de la ampliación

- El juego y Studio ejecutan la misma sesión C; WPF presenta sus píxeles.
- Probar no guarda el proyecto y no modifica el documento ni sus partidas.
- Una operación rechazada conserva el historial de rehacer.
- Cada gesto válido del mapa representa un único comando de deshacer.
- Las capacidades no terminadas no se presentan como entregadas.
- La medición de render CPU no se convierte en una promesa de FPS del juego.

## Aceptación pendiente del plan completo

### Implementado en esta iteración

- `retro_session.dll`: Player y Probar ejecutan la misma sesión C, con copia
  aislada del documento, pausa y avance de un tick. Probar no guarda en disco.
- Historial: una operación rechazada conserva rehacer y el estado de guardado.
- Archivo → Nuevo proyecto / Abrir proyecto. La plantilla mínima tiene una
  habitación y jugador; todavía no es el asistente FPS/terror definitivo.
- Mapa: dibujar habitaciones rectangulares, conectar automáticamente paredes
  compatibles, colocar puertas y ventanas sobre conexiones y elegir cota.
- Consultas 3D al colocar, mover o duplicar; otras alturas se muestran tenues.
- Biblioteca: fotogramas de atlas válidos y del arte provisional compartido con
  Player. Los objetos de inventario proceden de las definiciones del proyecto.
- Cuatro accesos de tarea, selector oscuro, texto base de 14 unidades y
  encuadrado del mapa. No se ha terminado la jerarquía de plantas y escaleras.
- Guardado de manifiesto, mapa, actores, reglas y diálogos como una transacción.
  Un diario pendiente se recupera antes de abrir el proyecto. Esto protege un
  guardado interrumpido; **no es autosave de cambios todavía no guardados**.

### Verificación reproducible

`tools/build.ps1 -Preset debug -Test` y el equivalente `release` incluyen CTest
y el ejecutable de pruebas .NET. La prueba de autoría crea un proyecto temporal
en una ruta con espacios, edita, deshace, rechaza un cambio, rehace, guarda,
reabre e inicia/detiene una prueba sin guardar sus cambios pendientes.

`tests/studio/Program.cs` compone el XAML con una ventana oculta y genera PNG a
1280×800 y 1920×1080 en `build/<preset>/studio-evidence`. Son renders de controles
WPF reales, **no capturas de una sesión humana ni prueba de dos monitores**.
Las pruebas C cubren además cruces de puertas, intervalos de paredes de distinta
longitud, plantas superpuestas, fallo de preparación y recuperación del diario.

Ejecución del 20-09-2026: Debug y Release compilaron sin advertencias y superaron
4/4 entradas de CTest. Player Release procesó 180 fotogramas en la posición
inicial de Haunted: media completa 7,704 ms, p95 10,141 ms, máximo 22,006 ms;
sesión CPU media 7,514 ms, memoria reportada de sesión 9,89 MiB, 10 marcadores y
4 luces definidas. CPU leído del registro: AMD Ryzen 5 3600, 6 núcleos.
Comando: `retro_player.exe --project assets/studio/haunted.retro --smoke 180
--capture build/release/studio-evidence/player.png`.
Es una muestra estática con ventana oculta, sin audio nuevo y sin recorrido
representativo; no certifica todavía el presupuesto de todo el juego ni mide
por separado simulación, audio y presentación. Las cifras anteriores de otros
ejecutables no sustituyen esta verificación de la sesión compartida.

Siguen pendientes los demás sistemas del plan: polígonos, plantas explícitas,
escaleras editables como grupo, creación completa de contenido y eventos,
música/voces, superficies animadas, combate ampliado, campañas, autosave completo
y exportación selectiva. No se considera cerrada ninguna etapa sólo por estas
mejoras parciales.

La sesión humana de creación, las pruebas en dos monitores y la escucha de
audio deben registrarse explícitamente. Compilar y superar CTest no sustituye
esa comprobación.
