/* Mundo 2.5D: polígonos convexos CCW y conexiones entre aristas opuestas.
 * Los límites son contratos del formato, no búferes que puedan desbordarse.
 * Propietario: la aplicación. No contiene punteros a memoria externa. */
#ifndef RETRO_WORLD_H
#define RETRO_WORLD_H
#include "retro/math.h"
#include <stddef.h>

enum {
    RE_MAX_SECTORS = 64,
    RE_MAX_VERTICES = 16,
    RE_MAX_MARKERS = 128,
    RE_MAX_BARRIERS = 128,
    RE_MAX_MATERIALS = 16
};
typedef struct ReError {
    size_t line;
    char message[256];
} ReError;
typedef struct ReSector {
    float floor, ceiling, light;
    int wall_material, floor_material, ceiling_material;
    size_t count;
    ReVec2 vertices[RE_MAX_VERTICES];
    int neighbor[RE_MAX_VERTICES]; /* -1 = pared sólida. */
    int neighbor_edge[RE_MAX_VERTICES];
    /* Intervalo normalizado de la arista ocupado por la abertura. La pared
     * sigue siendo solida fuera de [portal_start, portal_end]. En mapas v1-v3
     * el cargador migra las conexiones antiguas al intervalo completo [0,1].
     *
     * La arista reciproca recorre el muro en sentido contrario; por eso su
     * intervalo equivalente es [1-end, 1-start]. Esta representacion unica se
     * comparte entre renderer, colision, rayos y navegacion. */
    float portal_start[RE_MAX_VERTICES];
    float portal_end[RE_MAX_VERTICES];
    size_t source_line;
} ReSector;
typedef struct ReMarker {
    char kind[24];       /* Etiqueta opaca: sólo el juego conoce su significado. */
    char id[32];         /* Identidad estable para editor, eventos y referencias. */
    char definition[64]; /* Recurso que interpreta la capa de gameplay. */
    int sector;
    ReVec3 position;
    float yaw;
    size_t source_line;
} ReMarker;
enum ReBarrierKind { RE_BARRIER_DOOR, RE_BARRIER_WINDOW };
enum ReTraceMask {
    RE_BLOCK_MOVEMENT = 1u << 0,
    RE_BLOCK_SIGHT = 1u << 1,
    RE_BLOCK_PROJECTILE = 1u << 2
};
typedef struct ReBarrier {
    char id[32];
    enum ReBarrierKind kind;
    int sector, edge, material;
    unsigned int blocks;
    float open_fraction; /* 0=cerrada/intacta, 1=abierta o destruida. */
    float health;        /* 0 significa indestructible. */
    size_t source_line;
} ReBarrier;
typedef struct ReWorld {
    ReSector sectors[RE_MAX_SECTORS];
    ReMarker markers[RE_MAX_MARKERS];
    ReBarrier barriers[RE_MAX_BARRIERS];
    size_t sector_count, marker_count, barrier_count;
    unsigned int format_version;
} ReWorld;
typedef struct ReBody {
    ReVec3 position; /* Centro XY y altura de los pies. */
    float radius, height, step_height, vertical_speed;
    int sector;
    bool grounded;
} ReBody;

/* Carga transaccional: en error, out conserva su contenido. err es obligatorio.
 * Archivo <= 1 MiB; nombres y líneas acotados; error incluye línea de origen. */
[[nodiscard]] bool re_world_load(const char *path, ReWorld *out, ReError *err);
/* Serializa siempre el formato v2. Usa reemplazo mediante archivo temporal;
 * nunca deja un mapa parcialmente escrito. */
[[nodiscard]] bool re_world_save_v2(const char *path, const ReWorld *world, ReError *err);
/* v3 conserva la geometría v2 y permite que markers representen objetos,
 * interactuables y prefabs interpretados por retro_gameplay. */
[[nodiscard]] bool re_world_save_v3(const char *path, const ReWorld *world, ReError *err);
/* v4 conserva paredes alrededor de puertas y ventanas mediante aberturas
 * parciales. Es el formato de escritura actual de Studio. */
[[nodiscard]] bool re_world_save_v4(const char *path, const ReWorld *world, ReError *err);
[[nodiscard]] bool re_world_validate(const ReWorld *world, ReError *err);
bool re_sector_contains(const ReSector *sector, ReVec2 point);
/* preferred evita ambigüedad exactamente sobre una frontera compartida. */
int re_world_sector(const ReWorld *world, ReVec2 point, int preferred);
/* Consulta no ambigua para volúmenes superpuestos: z pertenece a [suelo, techo). */
int re_world_sector_at(const ReWorld *world, ReVec3 point, int preferred);
/* horizontal_delta está en metros, no en metros/segundo. Integra velocidad Z
 * con dt y gravedad. Contrato: dt en (0,1/30], cuerpo válido dentro del mapa. */
void re_body_move(const ReWorld *world, ReBody *body, ReVec2 horizontal_delta, float dt,
                  float gravity);
/* d normalizado: retorna distancia al primer obstáculo, o max_distance.
 * Incluye paredes, planos y puertas (techos móviles). */
float re_world_raycast(const ReWorld *world, ReVec3 origin, ReVec3 direction, float max_distance);
typedef struct ReTraceHit {
    float distance;
    int sector, edge, barrier;
} ReTraceHit;
/* Variante por canal. Un vidrio puede bloquear proyectiles y movimiento sin
 * impedir visión; una puerta cerrada normalmente bloquea los tres canales. */
ReTraceHit re_world_trace(const ReWorld *world, ReVec3 origin, ReVec3 direction, float max_distance,
                          unsigned int mask);
int re_world_barrier_at(const ReWorld *world, int sector, int edge);
/* BFS sin asignaciones. Retorna la primera conexión hacia goal o -1.
 * height filtra portales cerrados y step limita desniveles ascendentes. */
int re_world_next_sector(const ReWorld *world, int start, int goal, float height, float step);
#endif
