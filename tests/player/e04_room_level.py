"""Render a saved E04 room recipe in the real GPU Player."""

import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path


STATS = re.compile(r"demo_3d frames=8 draws=(\d+) triangles=(\d+) uploads=(\d+)")


def run(player: Path, level: Path, capture: Path | None = None) -> tuple[int, int, int]:
    command = [str(player), "--level", str(level), "--smoke", "8"]
    if capture is not None:
        command += ["--capture", str(capture)]
    completed = subprocess.run(command, capture_output=True, text=True,
                               encoding="utf-8", errors="replace", timeout=90, check=False)
    output = completed.stdout + completed.stderr
    print(output)
    match = STATS.search(output)
    if completed.returncode != 0 or match is None:
        raise AssertionError(f"Player could not render {level}")
    return tuple(map(int, match.groups()))


def main() -> None:
    player, base_level, work_directory = map(Path, sys.argv[1:4])
    work_directory.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="e04-player-", dir=work_directory) as scratch:
        root = Path(scratch)
        document = json.loads(base_level.read_text(encoding="utf-8"))
        document["entities"].append({
            "id": "e0400000-0000-4000-8000-000000000001",
            "parent": None,
            "transform": {
                "position": [0, -6, 0],
                "rotation": [0, 0, 0, 1],
                "scale": [1, 1, 1],
            },
            "components": {
                "vestigio.room": {
                    "version": 1,
                    "vertices": [[-2, -2], [2, -2], [2, 2], [-2, 2]],
                    "floor_z": 0,
                    "wall_height": 3,
                    "wall_thickness": 0.2,
                    "openings": [
                        {"edge": 2, "kind": "door", "offset": 1.2, "width": 1.6,
                         "height": 2.2, "sill": 0},
                        {"edge": 1, "kind": "window", "offset": 0.8, "width": 1.1,
                         "height": 1.0, "sill": 1.0},
                    ],
                }
            },
        })
        saved = root / "e04-room.level.json"
        saved.write_text(json.dumps(document, ensure_ascii=False), encoding="utf-8")
        baseline = run(player, base_level)
        capture = work_directory / "e04-player.png"
        room = run(player, saved, capture)
        if room[0] <= baseline[0] or room[1] <= baseline[1]:
            raise AssertionError("The recipe did not add visible GPU geometry")
        if not capture.is_file() or capture.stat().st_size < 1000:
            raise AssertionError("Player did not capture the room")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError, subprocess.TimeoutExpired) as error:
        print(f"E04 Player integration failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
