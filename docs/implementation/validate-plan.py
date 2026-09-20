"""Validate the implementation backlog and its generated Markdown. No engine runs."""

import argparse
import itertools
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent
STATES = {"PLANNED", "IN_PROGRESS", "IMPLEMENTED", "VERIFIED", "INTEGRATED", "BLOCKED"}


def inspect(data):
    errors = []
    tasks = data.get("tasks", [])
    if data.get("schema_version") != 1 or not tasks:
        return ["Expected schema_version 1 and nonempty tasks"], []
    ids = [task.get("id") for task in tasks]
    if len(ids) != len(set(ids)) or any(not value for value in ids):
        return ["Missing or duplicate ticket IDs"], []
    by_id = {task["id"]: task for task in tasks}
    profiles = set(data["validation_profiles"])
    roles = set(data["roles"])
    for task in tasks:
        ticket = task["id"]
        for field in ("title", "phase", "role", "status", "unlocks"):
            if not isinstance(task.get(field), str) or not task[field].strip():
                errors.append(f"{ticket}: missing {field}")
        for field in ("steps", "acceptance", "paths", "locks", "validation"):
            values = task.get(field)
            if not isinstance(values, list) or not values or not all(isinstance(v, str) and v for v in values):
                errors.append(f"{ticket}: invalid {field}")
        if task["status"] not in STATES:
            errors.append(f"{ticket}: invalid status")
        if task["role"] not in roles:
            errors.append(f"{ticket}: unknown role")
        if not set(task["validation"]) <= profiles:
            errors.append(f"{ticket}: unknown validation profile")
        dependencies = task.get("dependencies")
        if not isinstance(dependencies, list):
            errors.append(f"{ticket}: dependencies must be a list")
            continue
        if len(dependencies) != len(set(dependencies)):
            errors.append(f"{ticket}: duplicate dependencies")
        for dependency in dependencies:
            if dependency not in by_id or dependency == ticket:
                errors.append(f"{ticket}: invalid dependency {dependency}")
        if task["status"] in {"IN_PROGRESS", "IMPLEMENTED", "VERIFIED", "INTEGRATED"}:
            if any(by_id.get(dep, {}).get("status") != "INTEGRATED" for dep in dependencies):
                errors.append(f"{ticket}: started before dependencies were integrated")
        if task["status"] in {"VERIFIED", "INTEGRATED"} and not task.get("evidence"):
            errors.append(f"{ticket}: verified status requires evidence references")
        if task["status"] == "INTEGRATED" and not task.get("integrated_commit"):
            errors.append(f"{ticket}: integrated status requires integrated_commit")
        if task["status"] == "BLOCKED" and not task.get("blocked_reason"):
            errors.append(f"{ticket}: blocked status requires blocked_reason")
    if errors:
        return errors, []
    pending = set(ids)
    complete = set()
    waves = []
    while pending:
        wave = [ticket for ticket in ids if ticket in pending and set(by_id[ticket]["dependencies"]) <= complete]
        if not wave:
            return ["Dependency cycle: " + ", ".join(sorted(pending))], []
        waves.append(wave)
        pending.difference_update(wave)
        complete.update(wave)
    scope = set(data.get("default_scope", []))
    if not scope or not scope <= set(ids):
        errors.append("Invalid default scope")
    else:
        for ticket in scope:
            if not set(by_id[ticket]["dependencies"]) <= scope:
                errors.append(f"Default scope omits dependencies of {ticket}")
    roots = [task["id"] for task in tasks if not task["dependencies"]]
    if roots != ["F00"]:
        errors.append("The initial dependency root must be F00 only")
    ancestors = {ticket: set() for ticket in ids}
    for wave in waves:
        for ticket in wave:
            for dependency in by_id[ticket]["dependencies"]:
                ancestors[ticket].add(dependency)
                ancestors[ticket].update(ancestors[dependency])
    if "Z01" not in ancestors or ancestors["Z01"] != set(ids) - {"Z01"}:
        errors.append("Final gate Z01 must transitively depend on every other task")
    return errors, waves


def task_markdown(data):
    lines = [
        "# Tickets de implementación", "",
        "Generado desde [backlog.json](backlog.json). Editar el JSON y ejecutar `python docs/implementation/validate-plan.py --render`; no mantener dos versiones manuales.", "",
        "Los paths son puntos de entrada; directorios nuevos son propuestas hasta F01. Antes de escribir se reclama una lista exacta de archivos. Los perfiles se definen en [VALIDATION.md](VALIDATION.md).", "",
    ]
    for task in data["tasks"]:
        lines += [f"## {task['id']} — {task['title']}", "",
                  f"Fase: **{task['phase']}** · Rol: **{task['role']}** · Estado: **{task['status']}**.", "",
                  "Dependencias integradas: " + (", ".join(task["dependencies"]) or "ninguna; inicio del plan") + ".", "",
                  "Locks: " + ", ".join(f"`{value}`" for value in task["locks"]) + ".", "",
                  "Puntos de entrada: " + ", ".join(f"`{value}`" for value in task["paths"]) + ".", "",
                  "**Trabajo:**", ""]
        lines += [f"{number}. {step}" for number, step in enumerate(task["steps"], 1)]
        lines += ["", "**Aceptación:**", ""]
        lines += [f"- {item}" for item in task["acceptance"]]
        lines += ["", "Verificación: " + ", ".join(task["validation"]) + ".", "",
                  "Desbloquea: " + task["unlocks"], ""]
        if task.get("evidence"):
            lines += ["Evidencia: " + ", ".join(task["evidence"]) + ".", ""]
        if task.get("integrated_commit"):
            lines += [f"Commit integrado: `{task['integrated_commit']}`.", ""]
        if task.get("blocked_reason"):
            lines += ["Bloqueo: " + task["blocked_reason"], ""]
    return "\n".join(lines)


def wave_markdown(data, waves):
    by_id = {task["id"]: task for task in data["tasks"]}
    lines = ["# Oleadas y dependencias", "",
             "Generado de [backlog.json](backlog.json). Una oleada expresa profundidad de dependencias, no una barrera obligatoria ni autorización para escribir simultáneamente. Se puede adelantar un ticket cuando sus dependencias estén INTEGRATED y sus locks/archivos estén libres.", "",
             "El coordinador puede usar menos workers que tickets. Además de locks se revisa el solapamiento real de archivos; ver [PLAN.md](PLAN.md).", "",
             "| Oleada teórica | Tickets | Conflictos de locks dentro de la oleada |", "|---|---|---|"]
    for index, wave in enumerate(waves):
        conflicts = []
        for a, b in itertools.combinations(wave, 2):
            common = sorted(set(by_id[a]["locks"]) & set(by_id[b]["locks"]))
            if common:
                conflicts.append(f"{a}/{b}: " + ", ".join(common))
        lines.append(f"| W{index:02d} | {', '.join(wave)} | {'; '.join(conflicts) or 'Ninguno declarado; verificar archivos'} |")
    lines += ["", "## DAG completo", "", "```mermaid", "flowchart TD"]
    for task in data["tasks"]:
        lines.append(f"  {task['id']}[{task['id']}]")
    for task in data["tasks"]:
        for dependency in task["dependencies"]:
            lines.append(f"  {dependency} --> {task['id']}")
    lines += ["```", "", "## Elegibilidad actual", ""]
    eligible = [task["id"] for task in data["tasks"] if task["status"] == "PLANNED" and
                all(by_id[dep]["status"] == "INTEGRATED" for dep in task["dependencies"])]
    lines += ["Por estado de dependencias: " + (", ".join(eligible) or "ningún ticket PLANNED elegible") + ".", "",
              "Filtrar después por alcance encargado y locks. BLOCKED requiere resolver su motivo y actualizar estado; no se relanza automáticamente.", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--render", action="store_true", help="Regenerate TASKS.md and WAVES.md after validation")
    args = parser.parse_args()
    try:
        data = json.loads((ROOT / "backlog.json").read_text(encoding="utf-8"))
        errors, waves = inspect(data)
    except (ValueError, TypeError, KeyError, OSError) as exc:
        print(f"FAIL: invalid backlog: {exc}")
        return 1
    if errors:
        for error in errors:
            print("FAIL: " + error)
        return 1
    generated = {"TASKS.md": task_markdown(data), "WAVES.md": wave_markdown(data, waves)}
    for name, expected in generated.items():
        destination = ROOT / name
        if args.render:
            destination.write_text(expected, encoding="utf-8", newline="\n")
        elif not destination.exists() or destination.read_text(encoding="utf-8") != expected:
            errors.append(f"{name} missing or stale; run --render")
    if errors:
        for error in errors:
            print("FAIL: " + error)
        return 1
    print(json.dumps({"result": "PASS", "tickets": len(data["tasks"]), "waves": len(waves),
                      "dependency_edges": sum(len(task["dependencies"]) for task in data["tasks"]),
                      "default_scope_tickets": len(data["default_scope"]),
                      "all_tasks_reach_final_gate": True, "engine_tests_run": False}, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
