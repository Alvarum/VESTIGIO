# VESTIGIO: entregables pequeños para un proyecto hobby

Estado al 2026-09-27: **16 de 30 tickets integrados**. El usuario revisó y aceptó J01 y S01/S02. La oleada 3 (E01) está [lista para tu revisión](evidence/E01.md). La oleada 4 ya tiene [flujo funcional de edición verificado](evidence/W04.md); E02 continúa `IN_PROGRESS` y E03 permanece `PLANNED` según sus criterios formales. El usuario encargó continuar con **cuatro oleadas consecutivas, 4–7**, con commit, pruebas, evidencia y push separados. A01 tiene sólo un núcleo de audio parcial.

**El motor es nuevo.** No se exige compatibilidad con Haunted, sectores, formatos previos, juegos anteriores o renderer CPU. Los tickets G04 (adaptador GPU anterior) y D03 (migración) se retiraron. El código de prototipo ya integrado puede permanecer mientras no estorbe, pero no es criterio de aceptación para entregas nuevas ni justificación para financiar una migración.

| Oleada de entrega | Valor que se obtiene | Tickets | Puerta de aceptación y punto de parada |
|---|---|---|---|
| **0. Base y plan — ahora** | Publicar la base ya integrada y este contrato de entrega | Documentación; 12 tickets ya integrados | Validador del plan y `git diff --check` pasan; commits locales y este ajuste se envían a `origin/main`. Aún no hay demo recorrible |
| **1. Primera escena 3D** | Abrir una escena **nueva** con suelo y objetos visuales, ver un modelo glTF/GLB y mover la cámara con controles en Player | **J01** | Comando reproducible, recorrido libre, frame final en GPU real, cierre limpio y captura. **Sin colisiones** |
| **2. Movimiento con colisión** | Recorrer esa escena sin atravesar suelo y objetos | **S01 → S02** | Contactos, pendiente/escalón y movimiento estables en pruebas dirigidas y recorrido manual |
| **3. Editor visible** | Abrir la misma escena en Studio, verla en GPU y alternar Editar/Probar | **E01** | Viewport, foco, resize y Play/Stop sin modificar el documento de edición |
| **4. Edición de objetos** | Colocar y transformar un modelo; guardar, reabrir y volver a jugar | **E02 → E03** | Round-trip y undo/redo demostrados en un proyecto nuevo |
| **5. Interacción y puertas** | Interactuar con una puerta visible y coherente con su colisión | **S03** | Abrir/cerrar desde Player; panel y collider coinciden; guardar/reabrir conserva la definición |
| **6. Efectos visuales** | Comparar la misma escena en estilo limpio y retro con GPU | **V01 → V02** | Luces/fog y perfil visual con ajustes persistentes; resize y frames normales sin readback |
| **7. Audio** | Oír ambiente y acción de puerta con controles de volumen | **A01** | Sonidos independientes, pausa/foco y reproducción real verificada; ausencia de dispositivo con diagnóstico |

Las oleadas 4–7 están encargadas en ese orden. Después se detiene la ejecución y se ofrecen por separado, sólo si el usuario lo encarga: animación (**A02**), herramientas editoriales avanzadas (**E04/E05**) y distribución (**P01/P02, Q01, T01, Z01** y dependencias necesarias). No iniciar el paquete entero por defecto. [WAVES.md](WAVES.md) enumera profundidad de dependencias técnicas; sus filas no son oleadas de entrega ni autorizan trabajo adicional.

**Contrato de cierre por oleada:** completar el alcance acordado y sus pruebas dirigidas; registrar resultados reales, limitaciones y pasos para probarlo; hacer commits de código/evidencia y push directo a `origin/main` tras comprobar que no se sobrescribe trabajo remoto. Para las oleadas **4–7**, el encargo del usuario autoriza continuar consecutivamente tras publicar cada una; enviar una actualización con SHA y pasos de prueba, y atender cualquier fallo que reporte antes de avanzar. Tras la 7, esperar un nuevo encargo. No hay límite fijo de tiempo; informar avances y no ampliar el alcance por cuenta propia. Puede haber commits internos de checkpoint, pero un hito incompleto no se presenta como aceptado. Los archivos parciales de S01 no se incluyen en la oleada 0 o 1.

Las filas describen **puertas funcionales de hobby**. Las fichas E02/E03/S03/V01/V02/A01 del backlog contienen criterios adicionales: marcar `INTEGRATED` sólo cuando se verifiquen todos; un entregable funcional puede publicarse con el ticket aún `IN_PROGRESS` y una lista explícita de criterios pendientes.

Para J01, S01/S02 y E01 se ejecutaron pruebas dirigidas y verificación GPU visual; una matriz completa de distribución queda para cuando exista candidato de esa entrega. `ALCANCE: COMPLETO` es visión de largo plazo, no autorización de trabajo continuo.
