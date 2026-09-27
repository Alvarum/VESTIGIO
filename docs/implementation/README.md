# VESTIGIO — paquete de implementación para agentes

**Paquete de implementación para agentes.** Este paquete convierte el [roadmap de investigación](../research/12-roadmap.md) en tickets con dependencias, alcance, responsabilidades, pruebas y criterios de cierre.

**Actualización 2026-09-27:** hay 15 de 30 tickets integrados. La entrega avanza por [oleadas funcionales revisables](ENTREGABLES-HOBBY.md), una a la vez. El usuario revisó la oleada 1 (J01); la oleada 2 (S01/S02) añade colisiones y salto a la demo GPU y espera su revisión antes de iniciar la oleada 3. Véanse las evidencias [S01](evidence/S01.md) y [S02](evidence/S02.md). Se retiraron los tickets de compatibilidad y migración de proyectos anteriores.

La prioridad confirmada es **render de geometría, materiales y efectos en GPU**. SDK C y Studio deben usar el mismo runtime. El renderer CPU previo no es requisito de compatibilidad para los entregables nuevos.

## Cómo empezar

1. Elegir una oleada de [ENTREGABLES-HOBBY.md](ENTREGABLES-HOBBY.md). Sólo ejecutar la encargada; no confundirla con las filas técnicas de `WAVES.md`.
2. El agente lee [PLAN.md](PLAN.md), [CONTRACTS.md](CONTRACTS.md) y [VALIDATION.md](VALIDATION.md), y verifica el estado real del repositorio.
3. Completar su gate, hacer commit y push a `origin/main`, entregar SHA e instrucciones para probar. Esperar la revisión del usuario antes de la siguiente; no hay límite fijo de tiempo.

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
