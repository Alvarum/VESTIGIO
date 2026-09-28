# VESTIGIO — paquete de implementación para agentes

**Paquete de implementación para agentes.** Este paquete convierte el [roadmap de investigación](../research/12-roadmap.md) en tickets con dependencias, alcance, responsabilidades, pruebas y criterios de cierre.

**Frontera del repositorio:** este plan gobierna únicamente **VESTIGIO**, en
`engines/vestigio/` (CMake, `src/`, `include/`, `tests/`, `studio/`,
`studio.tests/` y `assets/`). RetroForge vive en otro repositorio. `tools/`,
`.tools/`, `.deps/` y `.nuget/` en esta raíz pertenecen sólo a VESTIGIO.

**Actualización 2026-09-27:** hay **19 de 90 tickets integrados técnicamente**. La entrega avanza por [oleadas o tickets revisables](ENTREGABLES-HOBBY.md), uno a la vez salvo encargo explícito de varios. El usuario revisó J01 y S01/S02. E01 abrió el Atrium en Studio; las oleadas 4–9 añadieron funciones jugables parciales. Después se implementaron [E02](evidence/E02.md), [E03](evidence/E03.md) y [E04](evidence/E04.md) con pruebas técnicas. **El usuario rechazó la usabilidad del Studio 3D mostrado; esa revisión queda pendiente de corrección.** Se retiraron los tickets de compatibilidad y migración de proyectos anteriores.

**Ampliación de investigación 2026-09-27:** la [auditoría integral de `js-game`](../research/13-js-game-full-audit.md) y su [inventario por archivo](../research/14-js-game-inventory.md) añaden 60 tickets `PLANNED`, sin ejecutar esas funciones. El backlog tiene ahora **90 tickets: 19 INTEGRATED, 4 IN_PROGRESS y 67 PLANNED**. `Z01` conserva el cierre original de 30 tickets; `ZA1` cierra la ampliación. La demo antigua sigue `NOT_RUN` en navegador. Cada ticket añadido requiere una entrega propia.

La prioridad confirmada es **render de geometría, materiales y efectos en GPU**. SDK C y Studio deben usar el mismo runtime. El renderer CPU previo no es requisito de compatibilidad para los entregables nuevos.

## Cómo empezar

1. Elegir una oleada o ticket de [ENTREGABLES-HOBBY.md](ENTREGABLES-HOBBY.md). Sólo ejecutar lo encargado; no confundirlo con las filas técnicas de `WAVES.md`.
2. El agente lee [PLAN.md](PLAN.md), [CONTRACTS.md](CONTRACTS.md) y [VALIDATION.md](VALIDATION.md), y verifica el estado real del repositorio.
3. Completar todos los criterios del ticket si el encargo pide cerrarlo; verificar, hacer commit y push a `origin/main`, entregar SHA e instrucciones para probar. Tras E02 se espera un nuevo encargo. No hay límite fijo de tiempo.

No es necesario copiar toda la investigación en un prompt. Entregar acceso al repositorio y este archivo con el prompt elegido basta; cada ticket indica sus lecturas y verificaciones a través del plan.

## Archivos y autoridad

| Archivo | Uso |
|---|---|
| [PLAN.md](PLAN.md) | Secuencia de entrega, delegación, integración y límites |
| [TASKS.md](TASKS.md) | Fichas legibles de cada ticket, generadas del backlog |
| [backlog.json](backlog.json) | Fuente única de tickets, dependencias, locks, estado y aceptación |
| [WAVES.md](WAVES.md) | Oleadas mínimas por dependencias; no autorización automática de simultaneidad |
| [CONTRACTS.md](CONTRACTS.md) | Fronteras que F01 debe fijar antes de repartir implementación |
| [VALIDATION.md](VALIDATION.md) | Comandos existentes, perfiles de pruebas y requisitos de evidencia |
| [AGENT-PROMPTS.md](AGENT-PROMPTS.md) | Prompts para ejecutar, coordinar, delegar y revisar |
| [STATE.md](STATE.md) | Estado global, base observada y punto exacto para retomar |
| [validate-plan.py](validate-plan.py) | Valida DAG/estados/cobertura y sincronización de documentos generados |

Instrucción explícita del usuario → contratos y tickets de implementación → investigación de diseño → documentación histórica. Una decisión nueva que cambie una frontera debe actualizar contrato y dependencias con su motivo; no convertir una elección de implementación local en un cambio unilateral de arquitectura.

Desde la raíz del repositorio:

```powershell
python docs/implementation/validate-plan.py
./tools/build.ps1 -Preset debug -Test
```

El build escribe en `build/vestigio/debug/`; los perfiles de análisis y UBSan
se seleccionan con `-Preset analyze` o `-Preset ubsan`.

Para regenerar las fichas y oleadas después de editar el backlog:

```powershell
python docs/implementation/validate-plan.py --render
```

Este validador comprueba el plan, **no el motor**. Los comandos de build/test se ejecutarán en F00 y en los tickets correspondientes.
