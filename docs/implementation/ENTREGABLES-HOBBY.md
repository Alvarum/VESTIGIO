# VESTIGIO: entregables pequeños para un proyecto hobby

Estado al 2026-09-27: **12 de 30 tickets integrados**. Ya existen la base GPU real, SDK C instalable, importación glTF/GLB, input/settings y documento transaccional. A01 tiene sólo un núcleo de audio parcial. E01 y S01 quedaron interrumpidos; hay archivos parciales de S01 sin seguimiento en `src/physics/` y `src/world/spatial_world.*`, que deben revisarse antes de reutilizarlos.

**El motor es nuevo.** No se exige compatibilidad con Haunted, sectores, formatos previos, juegos anteriores o renderer CPU. Los tickets G04 (adaptador GPU anterior) y D03 (migración) se retiraron. El código de prototipo ya integrado puede permanecer mientras no estorbe, pero no es criterio de aceptación para entregas nuevas ni justificación para financiar una migración.

| Oleada de entrega | Valor que se obtiene | Tickets | Puerta de aceptación y punto de parada |
|---|---|---|---|
| **0. Base y plan — ahora** | Publicar la base ya integrada y este contrato de entrega | Documentación; 12 tickets ya integrados | Validador del plan y `git diff --check` pasan; commits locales y este ajuste se envían a `origin/main`. Aún no hay demo recorrible |
| **1. Primera escena 3D** | Abrir una escena **nueva** con suelo y objetos visuales, ver un modelo glTF/GLB y mover la cámara con controles en Player | **J01** | Comando reproducible, recorrido libre, frame final en GPU real, cierre limpio y captura. **Sin colisiones** |
| **2. Movimiento con colisión** | Recorrer esa escena sin atravesar suelo y objetos | **S01 → S02** | Contactos, pendiente/escalón y movimiento estables en pruebas dirigidas y recorrido manual |
| **3. Editor visible** | Abrir la misma escena en Studio, verla en GPU y alternar Editar/Probar | **E01** | Viewport, foco, resize y Play/Stop sin modificar el documento de edición |
| **4. Edición de objetos** | Colocar y transformar un modelo; guardar, reabrir y volver a jugar | **E02 → E03** | Round-trip y undo/redo demostrados en un proyecto nuevo |

Después de la oleada 4, ofrecer por separado y sólo si el usuario lo encarga: interacción/puertas (**S03**), efectos y perfil retro (**V01 → V02**), audio (**A01**), animación (**A02**), herramientas editoriales avanzadas (**E04/E05**) y distribución (**P01/P02, Q01, T01, Z01** y dependencias necesarias). No iniciar el paquete entero por defecto. [WAVES.md](WAVES.md) enumera profundidad de dependencias técnicas; sus filas no son oleadas de entrega ni autorizan trabajo adicional.

**Contrato de cierre por oleada:** completar el alcance acordado y sus pruebas dirigidas; registrar resultados reales, limitaciones y pasos para probarlo; hacer commits de código/evidencia y push directo a `origin/main` tras comprobar que no se sobrescribe trabajo remoto. Entregar SHA publicado y esperar la revisión del usuario antes de comenzar otra oleada. Si el usuario detecta un fallo, corregirlo dentro de la misma oleada y publicar otro commit. No hay límite fijo de tiempo; informar avances y no ampliar el alcance por cuenta propia. Puede haber commits internos de checkpoint, pero un hito incompleto no se presenta como aceptado. Los archivos parciales de S01 no se incluyen en la oleada 0 o 1.

Para J01 se ejecutan pruebas dirigidas del cambio y verificación GPU visual; una matriz completa de distribución queda para cuando exista candidato de esa entrega. `ALCANCE: COMPLETO` es visión de largo plazo, no autorización de trabajo continuo.
