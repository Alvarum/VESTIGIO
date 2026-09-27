#include "assets/import/model_ir.h"
#include "content/room_recipe.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static bool parse(const char *json, VgRoomRecipe *out) {
    VgJsonError error = {0};
    VgJsonNode *root = vg_json_parse(json, strlen(json), &error);
    char message[256] = {0};
    bool okay = root != NULL && vg_room_recipe_parse(root, out, message, sizeof(message));
    vg_json_destroy(root);
    return okay;
}

static void test_room_mesh_and_openings(void) {
    const char *json = "{\"version\":1,\"vertices\":[[-2,-2],[2,-2],[2,2],[0,2],[0,1],[-2,1]],"
                       "\"floor_z\":0,\"wall_height\":3,\"wall_thickness\":0.2,"
                       "\"openings\":[{\"edge\":0,\"kind\":\"door\",\"offset\":1.5,\"width\":1,"
                       "\"height\":2,\"sill\":0},{\"edge\":1,\"kind\":\"window\",\"offset\":1,"
                       "\"width\":1,\"height\":1,\"sill\":1}]}";
    VgRoomRecipe recipe;
    CHECK(parse(json, &recipe));
    VgRoomMesh mesh = {0}, again = {0};
    char error[256];
    CHECK(vg_room_recipe_build(&recipe, &mesh, error, sizeof(error)));
    CHECK(vg_room_recipe_build(&recipe, &again, error, sizeof(error)));
    CHECK(mesh.vertex_count > 0u && mesh.vertex_count % 3u == 0u);
    CHECK(mesh.vertex_count == again.vertex_count);
    CHECK(memcmp(mesh.vertices, again.vertices, mesh.vertex_count * sizeof(*mesh.vertices)) == 0);
    bool has_floor = false, has_wall = false, fills_door = false, fills_window = false;
    for (uint32_t i = 0u; i < mesh.vertex_count; ++i) {
        const VgRoomVertex *v = &mesh.vertices[i];
        CHECK(isfinite(v->uv[0]) && isfinite(v->uv[1]));
        if (fabsf(v->position[2]) < 0.0001f && fabsf(v->position[0]) < 1.5f &&
            fabsf(v->position[1]) < 1.01f)
            has_floor = true;
        if (fabsf(v->position[1] + 2.0f) < 0.11f && v->position[2] > 2.9f)
            has_wall = true;
        if (fabsf(v->position[1] + 2.0f) < 0.11f && v->position[0] > -0.49f &&
            v->position[0] < 0.49f && v->position[2] > 0.01f && v->position[2] < 1.99f)
            fills_door = true;
        if (fabsf(v->position[0] - 2.0f) < 0.11f && v->position[1] > -0.99f &&
            v->position[1] < -0.01f && v->position[2] > 1.01f && v->position[2] < 1.99f)
            fills_window = true;
    }
    CHECK(has_floor && has_wall && !fills_door && !fills_window);
    void *packed = NULL;
    size_t bytes = 0u;
    CHECK(vg_room_mesh_model_ir(&mesh, &packed, &bytes));
    if (packed != NULL) {
        const VgStaticModelIr *model = packed;
        CHECK(model->magic == VG_MODEL_IR_MAGIC && model->total_bytes == bytes);
        CHECK(model->vertex_count == mesh.vertex_count);
        const VgModelVertexIr *render_vertices =
            VG_MODEL_IR_ARRAY_CONST(model, VgModelVertexIr, vertices);
        const uint32_t *indices = VG_MODEL_IR_ARRAY_CONST(model, uint32_t, indices);
        for (uint32_t i = 0u; i < mesh.vertex_count; ++i) {
            CHECK(indices[i] == i);
            CHECK(memcmp(render_vertices[i].position, mesh.vertices[i].position,
                         sizeof(render_vertices[i].position)) == 0);
            CHECK(memcmp(render_vertices[i].texcoord, mesh.vertices[i].uv,
                         sizeof(render_vertices[i].texcoord)) == 0);
        }
    }
    free(packed);
    vg_room_mesh_destroy(&mesh);
    vg_room_mesh_destroy(&again);
}

static void test_invalid_shapes_and_openings(void) {
    VgRoomRecipe recipe;
    const char *touching = "{\"version\":1,\"vertices\":[[0,0],[4,0],[4,4],[2,4],[2,0],[0,4]],"
                           "\"floor_z\":0,\"wall_height\":3,\"wall_thickness\":0.2}";
    const char *overlap = "{\"version\":1,\"vertices\":[[0,0],[4,0],[4,4],[0,4]],"
                          "\"floor_z\":0,\"wall_height\":3,\"wall_thickness\":0.2,"
                          "\"openings\":[{\"edge\":0,\"kind\":\"door\",\"offset\":1,\"width\":1,"
                          "\"height\":2,\"sill\":0},{\"edge\":0,\"kind\":\"gap\",\"offset\":1.5,"
                          "\"width\":1,\"height\":2,\"sill\":0}]}";
    const char *reversed = "{\"version\":1,\"vertices\":[[0,0],[0,4],[4,4],[4,0]],"
                           "\"floor_z\":0,\"wall_height\":3,\"wall_thickness\":0.2}";
    CHECK(!parse(touching, &recipe));
    CHECK(!parse(overlap, &recipe));
    CHECK(!parse(reversed, &recipe));
}

int main(void) {
    test_room_mesh_and_openings();
    test_invalid_shapes_and_openings();
    return failures == 0 ? 0 : 1;
}
