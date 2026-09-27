"""Regenerate the original Atrium cube mesh with flat face normals."""

import base64
import json
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
DESTINATION = ROOT / "assets" / "demo" / "atrium.gltf"

corners = [
    (-0.5, -0.5, -0.5), (0.5, -0.5, -0.5),
    (0.5, 0.5, -0.5), (-0.5, 0.5, -0.5),
    (-0.5, -0.5, 0.5), (0.5, -0.5, 0.5),
    (0.5, 0.5, 0.5), (-0.5, 0.5, 0.5),
]
faces = [
    ((0, 3, 2, 1), (0.0, 0.0, -1.0)),
    ((4, 5, 6, 7), (0.0, 0.0, 1.0)),
    ((0, 1, 5, 4), (0.0, -1.0, 0.0)),
    ((1, 2, 6, 5), (1.0, 0.0, 0.0)),
    ((2, 3, 7, 6), (0.0, 1.0, 0.0)),
    ((3, 0, 4, 7), (-1.0, 0.0, 0.0)),
]

positions = [corners[index] for indices, _ in faces for index in indices]
normals = [normal for _, normal in faces for _ in range(4)]
indices = [face * 4 + corner for face in range(6) for corner in (0, 1, 2, 0, 2, 3)]
position_data = b"".join(struct.pack("<3f", *value) for value in positions)
normal_data = b"".join(struct.pack("<3f", *value) for value in normals)
index_data = b"".join(struct.pack("<H", value) for value in indices)
payload = position_data + normal_data + index_data

document = json.loads(DESTINATION.read_text(encoding="utf-8"))
document["asset"]["generator"] = "VESTIGIO Atrium original procedural asset"
for mesh in document["meshes"]:
    primitive = mesh["primitives"][0]
    primitive["attributes"] = {"POSITION": 0, "NORMAL": 1}
    primitive["indices"] = 2
document["buffers"] = [{
    "byteLength": len(payload),
    "uri": "data:application/octet-stream;base64," + base64.b64encode(payload).decode("ascii"),
}]
document["bufferViews"] = [
    {"buffer": 0, "byteOffset": 0, "byteLength": len(position_data), "target": 34962},
    {"buffer": 0, "byteOffset": len(position_data), "byteLength": len(normal_data), "target": 34962},
    {"buffer": 0, "byteOffset": len(position_data) + len(normal_data),
     "byteLength": len(index_data), "target": 34963},
]
document["accessors"] = [
    {"bufferView": 0, "componentType": 5126, "count": len(positions),
     "type": "VEC3", "min": [-0.5, -0.5, -0.5], "max": [0.5, 0.5, 0.5]},
    {"bufferView": 1, "componentType": 5126, "count": len(normals), "type": "VEC3"},
    {"bufferView": 2, "componentType": 5123, "count": len(indices),
     "type": "SCALAR", "min": [0], "max": [23]},
]
DESTINATION.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
