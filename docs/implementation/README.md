# VESTIGIO — paquete de implementación para agentes

**Listo para entregar a uno o varios agentes.** Este paquete convierte el [roadmap de investigación](../research/12-roadmap.md) en tickets con dependencias, alcance, responsabilidades, pruebas y criterios de cierre. Todavía no se ha implementado ninguno de estos tickets.

La prioridad confirmada es **render de geometría, materiales y efectos en GPU**. SDK C y Studio usan el mismo runtime. Mantener el renderer CPU como laboratorio no permite posponer GPU hasta el final.

## Cómo empezar

1. Entregar al agente el prompt de [AGENT-PROMPTS.md](AGENT-PROMPTS.md), opción individual o coordinador.
2. El agente lee [PLAN.md](PLAN.md), [CONTRACTS.md](CONTRACTS.md) y [VALIDATION.md](VALIDATION.md).
3. Ejecuta **F00 → F01** antes de repartir código. Después GPU y runtime pueden avanzar en paralelo según dependencias y locks.
4. Primer encargo recomendado: **tramo ARRANQUE**, los ocho tickets F00/F01/G01/G02/R01/R02/R03/I01. Entrega superficie GPU viable y SDK mínimo; todavía no promete modelos importados ni editor 3D completo.
5. Al terminar ese tramo, se entrega evidencia y estado. El siguiente encargo puede autorizar `COMPLETO` o los tickets concretos siguientes. Si desde el principio se pide `COMPLETO`, continuar hasta Z01 sin pedir autorización repetida para cada ticket ordinario.

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
```

Para regenerar las fichas y oleadas después de editar el backlog:

```powershell
python docs/implementation/validate-plan.py --render
```

Este validador comprueba el plan, **no el motor**. Los comandos de build/test se ejecutarán en F00 y en los tickets correspondientes.
