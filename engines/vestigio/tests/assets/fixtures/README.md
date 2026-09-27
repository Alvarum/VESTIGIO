# M01 importer fixtures

These tiny fixtures were authored for Vestigio and are released under CC0-1.0.
They contain generated triangle data and no third-party artistic content.
`fixture-manifest.json` records the exact SHA-256 of every test input.

- `static_scene.gltf`: indexed geometry, hierarchy/pivots, basis conversion,
  material alpha settings, sampler, and a base-color PNG data URI.
- `nonindexed.gltf`: non-indexed triangle and generated-normal path.
- `static_triangle.glb`: equivalent generated triangle in GLB 2.0 form.
- `external_dependencies.gltf`: relative external buffer and PNG; verifies
  dependency ordering, hashes, MIME inference, and resolver behavior.
- `required_extension.gltf`: unsupported required extension rejection.
- `out_of_bounds.gltf`: buffer-view bounds rejection.
- `nonfinite_transform.gltf` and `nonfinite_material.gltf`: numeric overflow
  must not publish non-finite IR values.
- `truncated.gltf`: truncated JSON rejection.
