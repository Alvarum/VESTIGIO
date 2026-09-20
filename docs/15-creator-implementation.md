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

La sesión humana de creación, las pruebas en dos monitores y la escucha de
audio deben registrarse explícitamente. Compilar y superar CTest no sustituye
esa comprobación.
