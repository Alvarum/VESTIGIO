"""Snapshot the local js-game sources for the VESTIGIO research appendix.

This reads the old project only. It never writes to that project. Import
reachability is static evidence, not proof that a feature worked in a browser.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re


HERE = Path(__file__).resolve().parent
GROUPS = {
    "G01": "Renderer",
    "R01": "Entity",
    "R02": "AssetLoader PropLoader",
    "R03": "Camera Assert",
    "P01": "SaveManager",
    "I01": "Input",
    "I02": "TouchInput",
    "S01": "PhysicsWorld",
    "S02": "FPSController",
    "A02": "AnimationController Furniture",
    "H01": "SceneEditor",
    "H03": "PropFactory",
    "H04": "LightPlacer FlickerLight",
    "H05": "WallHoleTool",
    "H06": "FloorManager",
    "H07": "TriggerZonePainter TriggerZone",
    "H08": "SceneLoader SceneManager",
    "H09": "RoomBuilder",
    "V03": "ExteriorFog FogShaders",
    "V04": "VolumetricFog",
    "V05": "PostProcessor HorrorFXShader PS1Shader HeatDistortionShader",
    "V06": "FluidShader NeonShader OrganicShader PuddleShader TVStaticShader HolographicShader",
    "V07": "ParticleSystem",
    "V08": "FireSystem",
    "V09": "DecalSystem BreakableGlass",
    "V10": "WaterSystem",
    "V11": "SkySystem",
    "V12": "Weather",
    "V13": "VegetationSystem",
    "V14": "MirrorSystem",
    "V15": "LensFlare",
    "V16": "VideoPlayer",
    "V17": "FleshInfestation",
    "K01": "InteractionSystem",
    "K02": "EventBus StateMachine",
    "K03": "ScriptEngine",
    "K04": "Timeline CameraDolly",
    "K06": "CarrySystem",
    "K07": "AIController",
    "K08": "PlayerVitals",
    "K09": "Flashlight",
    "K10": "Weapon",
    "K11": "Ragdoll",
    "A01": "AudioManager",
    "A03": "AudioWorld",
    "A04": "ReverbZones",
    "A05": "FootstepSystem",
    "A06": "MusicDirector",
    "U01": "DialogManager SubtitleUI",
    "U02": "ChoiceUI ContextMenu",
    "U03": "InventoryUI",
    "U04": "ItemInspectUI ItemRevealUI",
    "U05": "DocumentUI CodexUI",
    "U06": "HUD DiegeticHUD",
    "U07": "PhoneUI",
    "U08": "Theme i18n",
    "U09": "SettingsMenu UIFocusManager ScreenManager PauseMenu LoadingScreen",
    "Q01": "Debug",
    "K05": "PuzzleManager CombinationLockUI PuzzleHelpers TerminalUI",
}
OWNER = {name + ".js": ticket for ticket, names in GROUPS.items() for name in names.split()}


def rel(source, path):
    return path.relative_to(source).as_posix()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()[:12]


def clean(text, maximum=130):
    return re.sub(r"\s+", " ", text.replace("|", "/").replace("`", "'")).strip()[:maximum]


def summary(path):
    text = path.read_text(encoding="utf-8")
    if path.name == "ExteriorFog.js":
        return "Capas de niebla exterior, filamentos, viento y motas con shader procedural"
    if path.parent.name == "rooms":
        return f"Composición de props e interacciones de la habitación {path.stem}"
    if path.name == "ApartmentLevel.js":
        return "Construcción de la escena del apartamento, puertas y habitaciones"
    for line in text.splitlines()[:35]:
        value = re.sub(r"^\s*(?:/\*+|\*|//|#)\s*", "", line).lstrip("\ufeff").strip()
        if " — " in value or " - " in value:
            return clean(value)
    for line in text.splitlines()[:35]:
        value = re.sub(r"^\s*(?:/\*+|\*|//|#)\s*", "", line).lstrip("\ufeff").strip()
        if len(value) > 18 and not value.startswith(("import ", "@", "export ")):
            return clean(value)
    return path.stem


def lifecycle(path):
    text = path.read_text(encoding="utf-8")
    methods = re.findall(
        r"(?m)^\s{4,}(?:async\s+)?(constructor|mount|init|load|save|update|destroy|dispose|unmount|render|play|pause|stop)\s*\(",
        text,
    )
    return ", ".join(dict.fromkeys(methods)) or "—"


def imports(path, source):
    content = path.read_text(encoding="utf-8")
    found = []
    for target in re.findall(r"\bfrom\s*['\"](\.[^'\"]+)['\"]|\bimport\s*['\"](\.[^'\"]+)['\"]", content):
        target = target[0] or target[1]
        candidate = (path.parent / target).resolve()
        if candidate.suffix == "":
            candidate = candidate.with_suffix(".js")
        if candidate.is_file() and candidate.is_relative_to(source):
            found.append(candidate)
    return found


def reachable(root, source):
    seen = set()
    pending = [root]
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        seen.add(current)
        pending.extend(imports(current, source))
    return seen


def plan_owner(path):
    label = path.as_posix().lower()
    if "01-html-renderer-base" in label:
        return "G01"
    if "02-camera-fps-controller" in label:
        return "S02"
    if "05.6-propfactory" in label:
        return "H03"
    if "13.8-animation" in label:
        return "A02"
    if "13.9-sceneeditor" in label:
        return "H01"
    if "13.95-engine-observability" in label:
        return "Q01"
    if "06-core-interactables" in label:
        return "K01"
    if "36-engine-showcase" in label or "sandbox-black-screen" in label:
        return "X01"
    if "phase-39" in label or "luces" in label:
        return "H04"
    if "requirements" in label or "features" in label or "roadmap" in label:
        return "ZA1"
    return "ZA1"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    engine = sorted((source / "engine").rglob("*.js"))
    game = sorted((source / "game").rglob("*.js"))
    plans = sorted(p for p in (source / ".planning").rglob("*") if p.is_file())
    assets = sorted((source / "assets/models").rglob("*.glb"))
    if (len(engine), len(game), len(plans), len(assets)) != (90, 18, 63, 105):
        raise SystemExit("Source inventory changed: review counts and mappings before regenerating")
    missing = [rel(source, p) for p in engine if p.name not in OWNER]
    if missing:
        raise SystemExit("Missing module owner: " + ", ".join(missing))
    casa = reachable(source / "game/main.js", source)
    sandbox = reachable(source / "game/sandbox/main.js", source)
    backlog = json.loads((HERE.parent / "implementation/backlog.json").read_text(encoding="utf-8"))
    tickets = {ticket["id"] for ticket in backlog["tasks"]}
    if not set(OWNER.values()) <= tickets:
        raise SystemExit("Module owner missing from backlog")
    manifest = json.loads((source / "assets/models/manifest.json").read_text(encoding="utf-8"))
    listed = {entry["file"].replace("\\", "/") for entry in manifest["models"]}
    on_disk = {path.relative_to(source / "assets/models").as_posix() for path in assets}
    if listed != on_disk:
        raise SystemExit(f"GLB manifest mismatch: unlisted={sorted(on_disk-listed)}, missing={sorted(listed-on_disk)}")
    consumer_paths = engine + game + sorted((source / "game").rglob("*.json"))
    consumer_text = {path: path.read_text(encoding="utf-8", errors="replace") for path in consumer_paths}
    lines = [
        "# Inventario trazable de `js-game`", "",
        "Generado por `python docs/research/build-js-game-inventory.py --source <ruta-js-game>`.",
        "Instantánea local: `bca6f57af2805809e87674297a2555da3b6a3957`; incluye tres resúmenes no versionados de fase 36.", "",
        "**Alcance:** 90 módulos de motor, 18 archivos de juego, 63 archivos `.planning`, 105 GLB. SHA-256 abreviado permite detectar cambios.",
        "`Conectado` significa alcanzable por imports estáticos desde `game/main.js` (Casa) o `game/sandbox/main.js` (Sandbox). No acredita ejecución, uso del símbolo ni calidad visual. `Código` significa que se encontró el archivo sin ruta estática desde esas entradas. `Sólo plan` nunca es prueba del motor.",
        "La disposición compara capacidades del **nuevo VESTIGIO**. `Parcial` exige el ticket indicado; `Existe` remite al ticket ya integrado; `Falta` indica ausencia de la capacidad específica.", "",
        "## 90 módulos de motor", "",
        "| Módulo | Intención localizada | Métodos de ciclo | Ruta estática | Evidencia | VESTIGIO | Ticket | SHA-256 |",
        "|---|---|---|---|---|---|---|---|",
    ]
    exists = {"G01", "R01", "R02", "R03", "I01", "S01", "S02"}
    partial = {"A01", "A02", "H01", "H04", "H09", "V05", "K01", "K02", "P01"}
    for path in engine:
        ticket = OWNER[path.name]
        route = "/".join(name for name, group in (("Casa", casa), ("Sandbox", sandbox)) if path in group) or "—"
        disposition = "Existe" if ticket in exists else "Parcial" if ticket in partial else "Falta"
        lines.append(f"| `{rel(source,path)}` | {summary(path)} | {lifecycle(path)} | {route} | {'Conectado' if route != '—' else 'Código'} | {disposition} | `{ticket}` | `{digest(path)}` |")
    lines += ["", "## 18 archivos de juegos y demos", "", "| Fuente | Papel | Ruta estática | Evidencia | Ticket | SHA-256 |", "|---|---|---|---|---|---|"]
    for path in game:
        route = "/".join(name for name, group in (("Casa", casa), ("Sandbox", sandbox)) if path in group) or "—"
        if path.name == "Day1Script.js":
            ticket = "K03"
        elif path.name in {"Door.js", "Interactables.js", "GameEvents.js"}:
            ticket = "K01"
        elif path.name == "LaCasaState.js":
            ticket = "K02"
        elif path.name == "lighting.js":
            ticket = "H04"
        elif path.name == "textures.js":
            ticket = "V06"
        else:
            ticket = "X01"
        lines.append(f"| `{rel(source,path)}` | {summary(path)} | {route} | {'Conectado' if route != '—' else 'Código'} | `{ticket}` | `{digest(path)}` |")
    lines += ["", "## 63 documentos de planificación", "", "| Documento | Tipo documental | Tema inicial | Evidencia | Ticket o decisión | SHA-256 |", "|---|---|---|---|---|---|"]
    for path in plans:
        name = path.name.upper()
        kind = "resumen histórico" if "SUMMARY" in name else "plan" if "PLAN" in name or "ROADMAP" in name else "requisito/idea" if "REQUIREMENT" in name or "RESEARCH" in name or "FEATURE" in name else "contexto/QA"
        heading = next((clean(line.lstrip("# "), 100) for line in path.read_text(encoding="utf-8").splitlines() if line.startswith("# ")), path.stem)
        lines.append(f"| `{rel(source,path)}` | {kind} | {heading} | Sólo plan | `{plan_owner(path)}` | `{digest(path)}` |")
    lines += ["", "## 105 modelos GLB", "", "`En manifest` indica presencia en el catálogo JSON, no uso en una escena. `Referencia literal` busca ruta, nombre o clave del modelo en JS/JSON de motor y juego, excluyendo el manifest; una ruta construida dinámicamente puede no aparecer. Fuente, autor, licencia y permiso de redistribución siguen sin prueba por archivo. Ningún modelo se copia a VESTIGIO en esta etapa.", "", "| GLB | Grupo | KiB | En manifest | Referencia literal | Procedencia | Ticket | SHA-256 |", "|---|---|---:|---|---|---|---|---|"]
    for path in assets:
        model_rel = path.relative_to(source / "assets/models").as_posix()
        candidates = (model_rel, path.name, model_rel[:-4])
        references = [rel(source, ref) for ref, text in consumer_text.items() if any(candidate in text for candidate in candidates)]
        used = ", ".join(f"`{ref}`" for ref in references[:2]) + (f" (+{len(references)-2})" if len(references) > 2 else "") if references else "—"
        lines.append(f"| `{rel(source,path)}` | {path.parent.name} | {path.stat().st_size / 1024:.0f} | {'Sí' if model_rel in listed else 'No'} | {used} | Sin verificar | `X02` | `{digest(path)}` |")
    lines += ["", "Los índices `assets/models/doors/index.json` y `assets/models/windows/index.json` contienen `[]`; los GLB de `doors and gates/` son archivos presentes, no una colección de puertas o ventanas especializada y operativa.", ""]
    (HERE / "14-js-game-inventory.md").write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(json.dumps({"engine":len(engine),"game":len(game),"plans":len(plans),"glb":len(assets),"manifest_glb":len(listed),"connected_engine":sum(p in casa or p in sandbox for p in engine)}))


if __name__ == "__main__":
    main()
