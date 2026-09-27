#ifndef VESTIGIO_CONTENT_ROOM_RECIPE_H
#define VESTIGIO_CONTENT_ROOM_RECIPE_H

#include "content/json.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    VG_ROOM_MAX_VERTICES = 24,
    VG_ROOM_MAX_OPENINGS = 48,
    VG_ROOM_MAX_TRIANGLES = 4096,
    VG_ROOM_MODEL_IMPORTER_VERSION = 2
};

typedef enum VgRoomOpeningKind { VG_ROOM_DOOR, VG_ROOM_WINDOW, VG_ROOM_GAP } VgRoomOpeningKind;

typedef struct VgRoomOpening {
    uint32_t edge;
    VgRoomOpeningKind kind;
    float offset, width, height, sill;
} VgRoomOpening;

typedef struct VgRoomRecipe {
    uint32_t vertex_count, opening_count;
    float vertices[VG_ROOM_MAX_VERTICES][2];
    float floor_z, wall_height, wall_thickness;
    VgRoomOpening openings[VG_ROOM_MAX_OPENINGS];
} VgRoomRecipe;

typedef struct VgRoomVertex {
    float position[3];
    float uv[2];
} VgRoomVertex;

typedef struct VgRoomMesh {
    VgRoomVertex *vertices;
    uint32_t vertex_count;
} VgRoomMesh;

/* A bounded, single-source recipe. Geometry is a triangle list; the same
 * positions feed rendering and the static-mesh collider. Coordinates are
 * right-handed Z-up metres. No derived data is serialized in the level. */
bool vg_room_recipe_parse(const VgJsonNode *node, VgRoomRecipe *out, char *error,
                          size_t error_capacity);
/* recipe must come from vg_room_recipe_parse. */
bool vg_room_recipe_build(const VgRoomRecipe *recipe, VgRoomMesh *out, char *error,
                          size_t error_capacity);
void vg_room_mesh_destroy(VgRoomMesh *mesh);
/* Packed static-model IR built from exactly the same triangles as collision. */
bool vg_room_mesh_model_ir(const VgRoomMesh *mesh, void **out_data, size_t *out_bytes);

#endif
