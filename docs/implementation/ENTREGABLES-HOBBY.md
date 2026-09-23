# VESTIGIO: entregables pequeños para un proyecto hobby

Estado al 2026-09-22: **12 de 30 tickets integrados**. Ya existen la base GPU real, SDK C instalable, importación glTF/GLB, input/settings y documento transaccional. A01 tiene sólo un núcleo de audio parcial. E01 y S01 quedaron interrumpidos; hay archivos parciales de S01 sin seguimiento en `src/physics/` y `src/world/spatial_world.*`, que deben revisarse antes de reutilizarlos. Ningún agente sigue ejecutando trabajo de motor.

**El motor es nuevo.** No se exige compatibilidad con Haunted, sectores, formatos previos, juegos anteriores o renderer CPU. Los tickets G04 (adaptador GPU anterior) y D03 (migración) se retiraron. El código de prototipo ya integrado puede permanecer mientras no estorbe, pero no es criterio de aceptación para entregas nuevas ni justificación para financiar una migración.

| Entregable | Valor que se obtiene | Tickets | Puerta de aceptación y punto de parada |
|---|---|---|---|
| **0. Base técnica — hecha** | GPU, SDK, assets 3D y documento listos para usarse | 12 integrados | Evidencia existente por ticket; aún no es una demo recorrible |
| **1. Primera escena 3D — siguiente recomendado** | Abrir una escena **nueva** con suelo y objetos visuales, ver un modelo glTF/GLB y mover la cámara con controles en Player | **J01** | Comando reproducible, recorrido libre, frame final en GPU real, cierre limpio y captura. **Sin colisiones**; detenerse aquí si basta para el hobby |
| **2. Movimiento con colisión — opcional** | Recorrer esa escena sin atravesar suelo y objetos | **S01 → S02** | Contactos, pendiente/escalón y movimiento estables en pruebas dirigidas y recorrido manual |
| **3. Editor visible — opcional** | Abrir la misma escena en Studio, verla en GPU y alternar Editar/Probar | **E01** | Viewport, foco, resize y Play/Stop sin modificar el documento de edición |
| **4. Edición de objetos — opcional** | Colocar y transformar un modelo; guardar, cerrar, reabrir y volver a jugar | **E02 → E03** | Round-trip y undo/redo demostrados en un proyecto nuevo |
| **5. Interacción y ambiente — opcional** | Puertas/triggers, luz/fog, perfil retro, audio o animación sólo según interés | **S03; V01 → V02; A01; A02** seleccionados por separado | Cada función elegida se ve y funciona en la escena nueva; no se compra todo el paquete por defecto |
| **6. Herramientas y distribución — opcional** | Herramientas de habitaciones, integración editorial completa, savegame, exportación y CLI | **E04/E05, P01/P02, Q01, T01, Z01** y sus dependencias cuando realmente se encarguen | Aceptación por función; exportación de dos juegos sólo si el proyecto llega a necesitarla |

Cada encargo abre **una sola ventana de hasta 90 minutos con un agente** y se detiene al acabarla, incluso si el ticket sigue incompleto. Un límite monetario o de tokens menor fijado por el usuario manda. El límite de tiempo controla una sesión de trabajo, **no garantiza precio ni finalización**; se informa avance y resultado real. No se inicia el entregable siguiente sin un encargo nuevo. Delegación paralela sólo si el usuario la solicita para ese entregable y dentro de su presupuesto.

Para el entregable 1 se ejecutan pruebas dirigidas del cambio y verificación GPU visual; una matriz completa de publicación queda para cuando exista candidato de entrega. `ALCANCE: COMPLETO` es ahora visión de largo plazo, no autorización de trabajo continuo.
