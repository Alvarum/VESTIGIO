# Investigación arquitectónica de VESTIGIO

Investigación realizada el 2026-09-20. **VESTIGIO debería ser un runtime retro 3D en C con render principal en GPU, SDK público y Studio como editor sobre el mismo runtime.** FPS/horror son especializaciones opcionales; Neo-PSX es un perfil gráfico. Editor-first y code-first deben producir contenido equivalente.

La propuesta incorpora la aclaración explícita de usar GPU. Preserva el valor del motor/editor existentes y sitúa el renderer CPU como laboratorio/regresión, sin limitar las capacidades GPU.

Para comenzar a implementar con uno o varios agentes, usar el [paquete de ejecución](../implementation/README.md): tickets, dependencias, contratos, pruebas y prompts de delegación. El roadmap de esta investigación conserva la justificación arquitectónica.

## Ruta de lectura

1. [Auditoría del engine actual](01-current-engine-audit.md): baseline, dependencias, ownership, deuda y partes a conservar.
2. [Arqueología de js-game](02-threejs-engine-archeology.md): workflows recuperables y fallos que no portar.
3. [Referencias externas](03-reference-engines.md): preguntas selectivas, archivos, símbolos e ideas.
4. [Matriz de capacidades](04-capability-matrix.md): evidencia comparada y gaps.
5. [Procedencia y licencias](05-source-provenance.md): sin código externo incorporado.
6. [Arquitectura objetivo](06-target-architecture.md): identidad, capas, entidades, GPU y bisagras.
7. [Public C API / SDK](07-public-c-api.md): frontera, lifecycle, ownership y alternativas.
8. [Arquitectura del editor](08-editor-architecture.md): documento, GPU/WPF, herramientas y UX.
9. [Pipeline de render](09-render-pipeline.md): GPU, perfiles, shaders, resolución y settings.
10. [Asset system](10-asset-system.md): caché, importación, CPU/GPU y audio.
11. [Proyecto y niveles](11-level-format.md): formatos, migraciones, persistencia y paquetes.
12. [Roadmap](12-roadmap.md): dependencias, resultados, aceptación y riesgos.

[Índice de fuentes](sources-index.md), [manifest de fuentes](source-manifest.json), [verificación de la investigación](verification.md).

## Alcance de esta entrega

Sólo documentación bajo `docs/research/`; ninguna feature ni refactor del engine. Auditoría estática: no se certifica ejecución, rendimiento GPU, importación real ni UX visual. Repositorio local auditado en `4e0cb9c866dcb2aa59e0e12a333c600ca12a390d`; el [anexo de cambios concurrentes](01-current-engine-audit.md#cambios-concurrentes-observados-al-cierre) distingue las mejoras posteriores observadas en `ae48d31` de ese baseline. Proyecto antiguo confirmado en `C:/Users/alvar/Documents/dev/js-game/`, no en la ruta inicial con `.planning` concatenado.

La decisión GPU es firme. El embedding en WPF, presupuesto gráfico, licencia pública del SDK y subconjunto ampliado de animación requieren los gates descritos; no se presentan como resueltos por investigación documental.
