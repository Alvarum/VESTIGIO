# 08 — Arquitectura del editor y workflows

Propuesta. Se preservan WPF/AvalonDock, documento nativo, historial y prueba aislada actuales. Cambia la superficie de render: GPU principal, no `WriteableBitmap` como contrato universal. Base: `src/studio/Controls/GameViewport.cs::Frame`, `MapViewport`, `StudioViewModel::BuildInspector`, `src/editor/editor.c::begin_command/commit_command`.

## Documento, vista y sesión

| Pieza | Posee | No posee |
|---|---|---|
| Document service C | Datos de proyecto/nivel, UUIDs, revisión, undo y transacciones | Objetos GPU ni widgets WPF |
| Asset service | Catálogo, dependencias, previews, residentes CPU/GPU | Selección/editor history |
| EditWorld | Proyección del documento a componentes renderizables/consultas | Estado definitivo de juego ni cambios independientes del documento |
| GameWorld de prueba | Copia del documento, game module y estado de simulación | Derecho a guardar de vuelta automáticamente |
| Viewport GPU | Cámara editorial, render targets, picking y gizmos | Reglas del juego o serializador paralelo |
| WPF ViewModels | Selección por ID, propiedades visibles, acciones/errores | Copia autorizada mutable de todo el nivel |

La Tool API publica cambios y actualiza EditWorld; loader/constructor comparte servicios con Public C API. Juego externo y editor no deben llamar exactamente la misma función de UI, pero sí consumir los mismos componentes, validadores, formatos y recursos.

## La integración GPU con WPF debe decidirse antes de ampliar el viewport

[HwndHost de Microsoft](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/hosting-win32-content-in-wpf) permite alojar contenido Win32 y exige lifecycle/foco propios. Los HWND tienen restricciones de composición conocidas como [airspace](https://learn.microsoft.com/en-us/dotnet/desktop/wpf/advanced/technology-regions-overview): no asumir que adornos WPF flotarán normalmente sobre la superficie nativa.

| Vía | Ventaja | Riesgo/decisión |
|---|---|---|
| Player GPU en ventana independiente | Implementación inicial pequeña, mismo renderer y assets que juego final | No entrega autoría 3D integrada; útil como primer spike y fallback de ejecución |
| HWND OpenGL propio alojado mediante HwndHost | GPU presenta directamente sin readback continuo | **Candidato preferido para viewport**, sujeto a prueba de contexto/ventana raylib, DPI/foco/docking |
| Copia GPU→RAM→WriteableBitmap | Fácil compatibilidad con UI existente | Sincronización/readback/conversión cada frame; sólo transición, thumbnails o captura |
| Textura GPU compartida con compositor WPF | Composición potencialmente integrada | Interop OpenGL/Direct3D y sincronización específica; demasiado coste para asumirla sin spike |
| Sustituir WPF por editor totalmente nativo | Una pila gráfica | Descarta editor funcional; sólo reevaluar si interop resulta inviable y coste está demostrado |

raylib `GetWindowHandle` entrega handle nativo, pero **no prueba soporte de adopción de un HWND ajeno ni múltiples ventanas/contextos**. Inspeccionar/adaptar `rcore`/backend de plataforma sólo después del spike. No especificar un `SetParent` aislado como solución terminada. La opción inicial mantiene una superficie nativa, un contexto gráfico, un hilo dueño y cámaras/targets múltiples en ese contexto. No depender de dos `InitWindow` simultáneos.

Spike verificable: abrir/cerrar editor y superficie 50 veces; redimensionar/minimizar; escalas DPI 100/150/200%; mover entre monitores; entrada ratón relativo/foco/Tab/Escape; abrir menús, docking y undocking; switch Edit/Play; render GPU confirmado; cero lectura de framebuffer por frame normal. Gizmos/overlays de viewport se dibujan en GPU; inspector/árbol siguen en WPF. Si sólo pasa ventana independiente, se entrega como experimento, no se cierra hito de autoría integrada.

## Modelo de comandos

Protocolo: Begin(expected revision)→Preview→Validate→Commit o Cancel. Preview de arrastre modifica estado efímero; Commit aplica una operación documental. Escape revierte. Un cambio rechazado no borra redo, regla que `begin_command/commit_command` actual ya respeta.

Comandos iniciales: CreateEntity, DeleteEntities, DuplicateEntities, SetTransform, Reparent, SetComponentProperty, SetMaterial, CreateRoom, EditOpening, ImportAsset, SetEnvironment. Mutaciones de múltiples objetos usan una transacción. IDs recién creados se devuelven en resultado; duplicación remapea referencias internas del grupo y conserva externas sólo según política explícita.

Snapshots completos siguen siendo válidos para documentos pequeños. Assets/mallas pesadas viven fuera. Si medición muestra coste, deltas before/after y checkpoints periódicos reemplazan implementación del historial sin romper Tool API. No cambiar a command objects en C++ ni añadir reflection universal por anticipación.

## Selección, cámara y herramientas

- SelectionSet de UUIDs, active entity y scope/layer; selección no altera dirty ni gameplay.
- Picking por raycast de editor contra bounds/mesh; máscara distinta de colisión de juego, para seleccionar luces, cámaras y triggers invisibles. Opción de pasar a través de objetos ocultos sólo explícita. GPU ID buffer puede añadirse si selección CPU no basta; evita readback sincrónico en cada mousemove.
- Modos Select/Move/Rotate/Scale/Place/Geometry, con interfaz lifecycle, preview, accept/cancel. Patrón de `EditMode` de Builder, no su implementación.
- Snapping central: unidades de distancia/ángulo/escala, surface snap, grid, local/world, pivot de grupo (activo/mediana/origen). Cota/plantas ayudan a organizar, no ocultan automáticamente otras plantas en runtime como `FloorManager` antiguo.
- Cámara de edición libre/orbit/ortográfica y encuadrar selección; separada de Camera component de juego y spawn del jugador.
- Gizmo de bisagra muestra eje, origen, arco y collider. Mover origen de malla no debe trasladar visual inesperadamente el panel: operación declara conservar world pose.

## Inspector, jerarquía y asset browser

Inspector deriva campos de metadatos: tipo/unidad/default/rango, slider opcional, referencia tipada por selector, ayuda y validación. Ediciones C# no serializan su propio JSON. Mantener lógica de validación en C; mostrar mensajes cerca del campo y en panel Problems con selección del objeto.

Hierarchy distingue organización (capas/grupos/plantas) de parent Transform. Ocultar/bloquear en editor no cambia visibilidad de juego salvo propiedad explícita. Multi-edit muestra valor mixto, conserva campos no editados y permite una sola acción undo.

Asset browser por tipos/carpetas/tags, búsqueda, estado de importación y dependencias faltantes. Drag→preview con ghost→click confirma; importar GLB muestra escala/ejes, materiales, bounds y subrecursos. Instanciar diez veces reutiliza GPU mesh/textures. Deshacer una colocación no elimina el archivo fuente; eliminar un asset requiere detectar referencias, no liberar memoria al azar.

## Recorridos de producto prioritarios

| Recorrido | Pasos | Criterio de aceptación |
|---|---|---|
| Habitación jugable | Nuevo→geometría/spawn→Probar→Stop | Mismo runtime; Stop conserva documento/undo; cámara de juego válida |
| Modelo importado | Importar→ver diagnóstico→colocar→TRS→guardar/reabrir | Transform, materiales, ID y collider preservados; no recarga por instancia |
| Puerta con llave | Hueco→bisagra/panel→key selector→sonido→probar | Estado/collider/sonido coherentes, apertura en ambos sentidos |
| Ambiente | Colocar luz/sonido→fog→perfil PSX | GPU refleja cambios; save/reload conserva parámetros; UI legible |
| Enemigo con interacción | Elegir definición→spawn→regla de drop/trigger | Referencias válidas y comportamiento idéntico al configurado por C |
| Código + editor | Juego registra tipo→Studio muestra esquema→editar→ejecutar | No se modifica engine para exponer propiedades de juego |
| Exportar | Validar→resolver assets→build juego→paquete | Ejecuta sin repo/.NET del editor; errores identifican dependencia concreta |

Console y Problems son distintos: log de diagnóstico frente a defectos accionables del documento. Contadores de entidades/meshes/luces/VRAM y overlays de collider/raycast/trigger se activan sin alterar el archivo. Undo, dirty, guardado, autosave y estado de importación deben mostrarse separadamente.

Accesibilidad mínima: foco visible, teclado para acciones principales, campos con unidades/nombres, contraste legible, targets de gizmo razonables y confirmación del estado guardado. No se asume calidad visual sin capturas de editor GPU real. La validación debe cubrir composición compacta, docking y distintos DPI, además de tests C.
