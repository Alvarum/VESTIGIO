#include "render/gpu_raylib/gpu_renderer.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { VG_GL_VENDOR = 0x1F00u, VG_GL_RENDERER = 0x1F01u, VG_GL_VERSION = 0x1F02u };
typedef const unsigned char *(*VgGlGetString)(unsigned int name);

struct VgGpuRenderer {
    RenderTexture2D target;
    Mesh cube;
    Material material;
    Texture2D sprite;
    Camera3D camera;
    VgFrameStats stats;
    uint32_t width, height;
};

static const char *vertex_shader = "#version 330\n"
                                   "in vec3 vertexPosition;\n"
                                   "in vec2 vertexTexCoord;\n"
                                   "in vec4 vertexColor;\n"
                                   "uniform mat4 mvp;\n"
                                   "out vec2 fragTexCoord;\n"
                                   "out vec4 fragColor;\n"
                                   "void main(){fragTexCoord=vertexTexCoord;fragColor=vertexColor;"
                                   "gl_Position=mvp*vec4(vertexPosition,1.0);}\n";

static const char *fragment_shader =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main(){vec4 texel=texture(texture0,fragTexCoord);"
    "if(texel.a<0.5) discard;finalColor=texel*colDiffuse*fragColor;}\n";

static void message(char *out, size_t capacity, const char *value) {
    if (out && capacity)
        (void)snprintf(out, capacity, "%s", value);
}

bool vg_gpu_renderer_validate_shader(const char *vs, const char *fs, char *error,
                                     size_t error_capacity) {
    if (!IsWindowReady() || !vs || !fs) {
        message(error, error_capacity, "Contexto grafico o fuente de shader invalida");
        return false;
    }
    Shader candidate = LoadShaderFromMemory(vs, fs);
    if (candidate.id == 0u || candidate.id == rlGetShaderIdDefault()) {
        if (candidate.locs && candidate.id == 0u)
            UnloadShader(candidate);
        message(error, error_capacity, "No se pudo compilar/enlazar shader GPU");
        return false;
    }
    UnloadShader(candidate);
    message(error, error_capacity, "");
    return true;
}

VgGpuRenderer *vg_gpu_renderer_create(VgGpuRendererConfig config, char *error,
                                      size_t error_capacity) {
    if (!IsWindowReady() || config.internal_width == 0u || config.internal_height == 0u ||
        config.internal_width > 4096u || config.internal_height > 4096u) {
        message(error, error_capacity, "Contexto o resolucion interna invalida");
        return NULL;
    }
    VgGpuRenderer *renderer = calloc(1u, sizeof(*renderer));
    if (!renderer) {
        message(error, error_capacity, "Sin memoria para renderer GPU");
        return NULL;
    }
    renderer->width = config.internal_width;
    renderer->height = config.internal_height;
    renderer->target = LoadRenderTexture((int)config.internal_width, (int)config.internal_height);
    if (renderer->target.id == 0u) {
        message(error, error_capacity, "No se pudo crear render target GPU");
        free(renderer);
        return NULL;
    }
    SetTextureFilter(renderer->target.texture, TEXTURE_FILTER_POINT);
    renderer->cube = GenMeshCube(1.5f, 1.5f, 1.5f);
    if (renderer->cube.vaoId == 0u) {
        message(error, error_capacity, "No se pudo subir mesh GPU");
        vg_gpu_renderer_destroy(renderer);
        return NULL;
    }
    renderer->material = LoadMaterialDefault();
    if (!renderer->material.maps) {
        message(error, error_capacity, "Sin memoria para material GPU");
        vg_gpu_renderer_destroy(renderer);
        return NULL;
    }
    Shader shader = LoadShaderFromMemory(vertex_shader, fragment_shader);
    if (shader.id == 0u || shader.id == rlGetShaderIdDefault()) {
        if (shader.locs && shader.id == 0u)
            UnloadShader(shader);
        message(error, error_capacity, "No se pudo crear shader principal GPU");
        vg_gpu_renderer_destroy(renderer);
        return NULL;
    }
    renderer->material.shader = shader;

    Image image = GenImageColor(4, 4, (Color){32, 132, 244, 255});
    ImageDrawRectangle(&image, 1, 1, 2, 2, BLANK);
    renderer->sprite = LoadTextureFromImage(image);
    UnloadImage(image);
    if (renderer->sprite.id == 0u) {
        message(error, error_capacity, "No se pudo subir textura de sprite GPU");
        vg_gpu_renderer_destroy(renderer);
        return NULL;
    }
    SetTextureFilter(renderer->sprite, TEXTURE_FILTER_POINT);

    renderer->camera = (Camera3D){.position = {0.0f, -5.0f, 1.6f},
                                  .target = {0.0f, 0.0f, 0.7f},
                                  .up = {0.0f, 0.0f, 1.0f},
                                  .fovy = 55.0f,
                                  .projection = CAMERA_PERSPECTIVE};
    renderer->stats.uploads = 3u;
    renderer->stats.estimated_gpu_bytes =
        (uint64_t)config.internal_width * (uint64_t)config.internal_height * 8u +
        (uint64_t)renderer->cube.vertexCount * 32u + 64u;
    message(error, error_capacity, "");
    return renderer;
}

void vg_gpu_renderer_destroy(VgGpuRenderer *renderer) {
    if (!renderer)
        return;
    if (renderer->sprite.id)
        UnloadTexture(renderer->sprite);
    if (renderer->material.maps)
        UnloadMaterial(renderer->material);
    if (renderer->cube.vaoId)
        UnloadMesh(renderer->cube);
    if (renderer->target.id)
        UnloadRenderTexture(renderer->target);
    free(renderer);
}

bool vg_gpu_renderer_draw_demo(VgGpuRenderer *renderer) {
    if (!renderer || !renderer->target.id)
        return false;
    BeginTextureMode(renderer->target);
    ClearBackground((Color){12, 18, 26, 255});
    BeginMode3D(renderer->camera);
    renderer->material.maps[MATERIAL_MAP_DIFFUSE].color = (Color){225, 62, 54, 255};
    DrawMesh(renderer->cube, renderer->material, MatrixTranslate(0.0f, 0.0f, 0.75f));
    renderer->material.maps[MATERIAL_MAP_DIFFUSE].color = (Color){54, 196, 103, 255};
    DrawMesh(renderer->cube, renderer->material, MatrixTranslate(0.0f, 1.2f, 0.75f));
    DrawBillboardRec(renderer->camera, renderer->sprite, (Rectangle){0, 0, 4, 4},
                     (Vector3){1.6f, 0.0f, 1.0f}, (Vector2){1.0f, 1.0f}, WHITE);
    EndMode3D();
    EndTextureMode();
    renderer->stats.frame_index++;
    renderer->stats.draw_calls = 3u;
    renderer->stats.triangles = 26u;
    return true;
}

void vg_gpu_renderer_present(VgGpuRenderer *renderer) {
    if (!renderer || !renderer->target.id)
        return;
    float width = (float)GetRenderWidth(), height = (float)GetRenderHeight();
    float scale = fminf(width / (float)renderer->width, height / (float)renderer->height);
    if (scale >= 1.0f)
        scale = floorf(scale);
    float draw_width = (float)renderer->width * scale;
    float draw_height = (float)renderer->height * scale;
    BeginDrawing();
    ClearBackground((Color){5, 8, 12, 255});
    DrawTexturePro(renderer->target.texture,
                   (Rectangle){0, 0, (float)renderer->width, -(float)renderer->height},
                   (Rectangle){(width - draw_width) * 0.5f, (height - draw_height) * 0.5f,
                               draw_width, draw_height},
                   (Vector2){0, 0}, 0.0f, WHITE);
    EndDrawing();
}

bool vg_gpu_renderer_capture(VgGpuRenderer *renderer, const char *path) {
    if (!renderer || !renderer->target.id || !path || !*path)
        return false;
    Image image = LoadImageFromTexture(renderer->target.texture);
    if (!image.data)
        return false;
    renderer->stats.readbacks++;
    ImageFlipVertical(&image);
    bool ok = ExportImage(image, path);
    UnloadImage(image);
    return ok;
}

bool vg_gpu_renderer_info(VgGpuInfo *out_info) {
    if (!out_info || !IsWindowReady())
        return false;
    void *address = rlGetProcAddress("glGetString");
    VgGlGetString get_string = NULL;
    if (!address || sizeof(get_string) != sizeof(address))
        return false;
    memcpy(&get_string, &address, sizeof(get_string));
    const unsigned char *vendor = get_string(VG_GL_VENDOR);
    const unsigned char *name = get_string(VG_GL_RENDERER);
    const unsigned char *version = get_string(VG_GL_VERSION);
    if (!vendor || !name || !version)
        return false;
    *out_info = (VgGpuInfo){.rlgl_version = rlGetVersion()};
    (void)snprintf(out_info->vendor, sizeof(out_info->vendor), "%s", (const char *)vendor);
    (void)snprintf(out_info->renderer, sizeof(out_info->renderer), "%s", (const char *)name);
    (void)snprintf(out_info->version, sizeof(out_info->version), "%s", (const char *)version);
    return true;
}

VgFrameStats vg_gpu_renderer_stats(const VgGpuRenderer *renderer) {
    return renderer ? renderer->stats : (VgFrameStats){0};
}
