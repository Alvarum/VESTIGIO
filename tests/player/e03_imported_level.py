"""Exercise a saved level with a second, imported GLB through the real GPU Player."""

import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> int:
    player, source_level, sample_glb, work_directory = map(Path, sys.argv[1:5])
    work_directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="e03-player-", dir=work_directory) as scratch:
        root = Path(scratch)
        assets = root / "assets"
        assets.mkdir()
        model = assets / "imported.glb"
        shutil.copyfile(sample_glb, model)

        level = json.loads(source_level.read_text(encoding="utf-8"))
        asset_id = "e0300000-0000-4000-8000-000000000001"
        entity_id = "e0300000-0000-4000-8000-000000000002"
        level["assets"] = [
            {
                "id": asset_id,
                "name": "Imported triangle",
                "source": "assets/imported.glb",
                "fingerprint": hashlib.sha256(model.read_bytes()).hexdigest(),
            }
        ]
        pillar = next(entity for entity in level["entities"] if "engine.mesh" in entity["components"])
        imported = json.loads(json.dumps(pillar))
        imported["id"] = entity_id
        imported["parent"] = None
        imported["transform"]["position"] = [0, -3, 1.6]
        imported["transform"]["scale"] = [1.8, 1.8, 1.8]
        imported["components"]["engine.mesh"]["asset"] = asset_id
        imported["components"]["engine.mesh"]["node_index"] = 0
        imported["components"].pop("engine.collider", None)
        imported.pop("required_components", None)
        level["entities"].append(imported)
        second = json.loads(json.dumps(imported))
        second["id"] = "e0300000-0000-4000-8000-000000000003"
        second["transform"]["position"] = [1.5, -3, 1.6]
        level["entities"].append(second)
        saved_level = root / "imported.level.json"
        saved_level.write_text(json.dumps(level, ensure_ascii=False), encoding="utf-8")

        baseline = subprocess.run(
            [str(player), "--level", str(source_level), "--smoke", "8"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=90,
            check=False,
        )
        baseline_stats = re.search(
            r"demo_3d frames=8 draws=(\d+) triangles=(\d+) uploads=(\d+)",
            baseline.stdout + baseline.stderr,
        )
        if baseline.returncode != 0 or baseline_stats is None:
            raise AssertionError("Atrium baseline did not render")
        capture = root / "player.png"
        completed = subprocess.run(
            [str(player), "--level", str(saved_level), "--smoke", "8", "--capture", str(capture)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=90,
            check=False,
        )
        output = completed.stdout + completed.stderr
        print(output)
        stats = re.search(r"demo_3d frames=8 draws=(\d+) triangles=(\d+) uploads=(\d+)", output)
        if (
            completed.returncode != 0
            or stats is None
            or int(stats.group(1)) <= int(baseline_stats.group(1))
            or int(stats.group(2)) <= int(baseline_stats.group(2))
            or int(stats.group(3)) != int(baseline_stats.group(3)) + 1
        ):
            raise AssertionError("Two imported instances did not draw with one shared GPU asset")
        if not capture.is_file() or capture.stat().st_size < 1000:
            raise AssertionError("Player did not capture the GPU frame")
        shutil.copyfile(capture, work_directory / "e03-player.png")

        # A modified source must not silently replace the model saved in the level.
        with model.open("ab") as stream:
            stream.write(b"changed")
        stale = subprocess.run(
            [str(player), "--level", str(saved_level), "--smoke", "1"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=90,
            check=False,
        )
        if stale.returncode == 0 or "cambio desde su importacion" not in stale.stdout + stale.stderr:
            raise AssertionError("Player accepted a source whose fingerprint changed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"E03 Player integration failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
