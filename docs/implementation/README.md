# VESTIGIO — paquete de implementación para agentes

**Paquete de implementación para agentes.** Este paquete convierte el [roadmap de investigación](../research/12-roadmap.md) en tickets con dependencias, alcance, responsabilidades, pruebas y criterios de cierre.

**Actualización 2026-09-22:** hay 12 de 30 tickets integrados. La ejecución se pausó para controlar gasto; consultar [entregables para hobby](ENTREGABLES-HOBBY.md) antes de encargar más código. Se retiraron los tickets de compatibilidad y migración de proyectos anteriores. El siguiente entregable recomendado es una escena 3D nueva recorrible en GPU.

La prioridad confirmada es **render de geometría, materiales y efectos en GPU**. SDK C y Studio deben usar el mismo runtime. El renderer CPU previo no es requisito de compatibilidad para los entregables nuevos.

## Cómo empezar

1. Elegir un entregable de [ENTREGABLES-HOBBY.md](ENTREGABLES-HOBBY.md). La base 0 ya está entregada; el siguiente recomendado es la demo jugable GPU.
2. El agente lee [PLAN.md](PLAN.md), [CONTRACTS.md](CONTRACTS.md) y [VALIDATION.md](VALIDATION.md), y verifica el estado real del repositorio.
3. Ejecuta una ventana acotada, entrega evidencia y estado al cierre. El siguiente trabajo requiere una nueva elección del usuario.

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
