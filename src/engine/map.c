/* Cargador de texto deliberadamente pequeño. No interpreta reglas del juego.
 * Fases: tokenizar -> convertir con control de rango -> validar -> publicar.
 * Nunca se utiliza atoi(): no permite distinguir cero de un error. */
#include "retro/world.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static bool fail(ReError *err, size_t line, const char *message) {
    err->line = line;
    (void)snprintf(err->message, sizeof(err->message), "%s", message);
    return false;
}

static bool replace_file(const char *temporary, const char *destination) {
#ifdef _WIN32
    return MoveFileExA(temporary, destination,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, destination) == 0;
#endif
}

static bool integer(const char *text, int *out) {
    char *end = nullptr;
    errno = 0;
    long value = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || value < INT_MIN || value > INT_MAX)
        return false;
    *out = (int)value;
    return true;
}
static bool number(const char *text, float *out) {
    char *end = nullptr;
    errno = 0;
    float value = strtof(text, &end);
    if (errno || end == text || *end != '\0' || !isfinite(value) || fabsf(value) > 10000)
        return false;
    *out = value;
    return true;
}

/* Los tokens son préstamos del búfer de línea, válidos hasta el siguiente fgets.
 * El cast a unsigned char evita UB en ctype para bytes UTF-8 mayores que 127. */
static size_t tokenize(char *line, char **tokens, size_t capacity) {
    size_t count = 0;
    char *cursor = line;
    while (*cursor) {
        while (isspace((unsigned char)*cursor))
            cursor++;
        if (!*cursor || *cursor == '#')
            break;
        if (count == capacity)
            return capacity + 1;
        tokens[count++] = cursor;
        while (*cursor && !isspace((unsigned char)*cursor) && *cursor != '#')
            cursor++;
        if (*cursor == '#') {
            *cursor = '\0';
            break;
        }
        if (*cursor)
            *cursor++ = '\0';
    }
    return count;
}

bool re_world_load(const char *path, ReWorld *out, ReError *err) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return fail(err, 0, "No se pudo abrir el mapa");
    /* Rechazamos NUL antes de usar cadenas C: strlen no debe ocultar bytes del
     * archivo. Esta pasada está acotada a 1 MiB y no requiere otra reserva. */
    size_t bytes = 0, source_line = 1;
    int byte;
    while ((byte = fgetc(file)) != EOF) {
        if (byte == 0 || ++bytes > 1024u * 1024u) {
            (void)fclose(file);
            return fail(err, source_line, "Byte NUL o archivo mayor que 1 MiB");
        }
        if (byte == '\n')
            source_line++;
    }
    if (ferror(file) || fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        return fail(err, source_line, "Error leyendo el archivo");
    }
    /* Un único bloque temporal permite dejar intacto el mundo anterior si falla.
     * ReWorld es demasiado grande para abusar de la pila de Windows. */
    ReWorld *candidate = calloc(1, sizeof(*candidate));
    if (!candidate) {
        (void)fclose(file);
        return fail(err, 0, "Memoria insuficiente");
    }
    bool ok = true, header = false;
    char line[1024];
    size_t line_number = 0, total = 0;
    int current = -1;
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        size_t length = strlen(line);
        total += length;
        if (total > 1024 * 1024 || (length == sizeof(line) - 1 && line[length - 1] != '\n')) {
            ok = fail(err, line_number, "Archivo o linea demasiado largos");
            break;
        }
        char *tokens[16];
        size_t count = tokenize(line, tokens, 16);
        if (count == 0)
            continue;
        if (!header) {
            int version = 0;
            if (count != 2 || strcmp(tokens[0], "retro_map") || !integer(tokens[1], &version) ||
                (version < 1 || version > 4)) {
                ok = fail(err, line_number, "Se esperaba retro_map 1, 2, 3 o 4");
                break;
            }
            candidate->format_version = (unsigned int)version;
            header = true;
            continue;
        }
        if (count == 8 && strcmp(tokens[0], "sector") == 0) {
            int id;
            if (!integer(tokens[1], &id) || id != (int)candidate->sector_count ||
                candidate->sector_count == RE_MAX_SECTORS) {
                ok = fail(err, line_number, "Sectores: IDs consecutivos desde 0, maximo 64");
                break;
            }
            ReSector *s = &candidate->sectors[candidate->sector_count];
            if (!number(tokens[2], &s->floor) || !number(tokens[3], &s->ceiling) ||
                !number(tokens[4], &s->light) || !integer(tokens[5], &s->wall_material) ||
                !integer(tokens[6], &s->floor_material) ||
                !integer(tokens[7], &s->ceiling_material)) {
                ok = fail(err, line_number, "Numero de sector invalido o fuera de rango");
                break;
            }
            for (size_t i = 0; i < RE_MAX_VERTICES; i++) {
                s->neighbor[i] = -1;
                s->neighbor_edge[i] = -1;
                s->portal_start[i] = 0;
                s->portal_end[i] = 1;
            }
            s->source_line = line_number;
            current = id;
            candidate->sector_count++;
        } else if (count == 3 && strcmp(tokens[0], "v") == 0 && current >= 0) {
            ReSector *s = &candidate->sectors[current];
            if (s->count == RE_MAX_VERTICES || !number(tokens[1], &s->vertices[s->count].x) ||
                !number(tokens[2], &s->vertices[s->count].y)) {
                ok = fail(err, line_number, "Vertice invalido o mas de 16 vertices");
                break;
            }
            s->count++;
        } else if ((count == 4 || count == 6) && strcmp(tokens[0], "link") == 0 && current >= 0) {
            int edge, neighbor, other;
            if (!integer(tokens[1], &edge) || !integer(tokens[2], &neighbor) ||
                !integer(tokens[3], &other) || edge < 0 ||
                edge >= (int)candidate->sectors[current].count || neighbor < 0 || other < 0 ||
                candidate->sectors[current].neighbor[edge] != -1) {
                ok = fail(err, line_number,
                          "Conexion invalida, duplicada o anterior a sus vertices");
                break;
            }
            if (candidate->format_version >= 4 && count != 6) {
                ok = fail(err, line_number,
                          "En retro_map 4 link necesita inicio y fin normalizados");
                break;
            }
            float start = 0, end = 1;
            if (count == 6 && (!number(tokens[4], &start) || !number(tokens[5], &end) ||
                               start < 0 || end > 1 || end - start < 0.01f)) {
                ok = fail(err, line_number,
                          "Intervalo de abertura invalido; use 0 <= inicio < fin <= 1");
                break;
            }
            candidate->sectors[current].neighbor[edge] = neighbor;
            candidate->sectors[current].neighbor_edge[edge] = other;
            candidate->sectors[current].portal_start[edge] = start;
            candidate->sectors[current].portal_end[edge] = end;
        } else if (((candidate->format_version == 1 && count == 7) ||
                    (candidate->format_version >= 2 && count == 9)) &&
                   strcmp(tokens[0], "spawn") == 0) {
            if (candidate->marker_count == RE_MAX_MARKERS || strlen(tokens[1]) >= 24) {
                ok = fail(err, line_number, "Demasiados marcadores o etiqueta demasiado larga");
                break;
            }
            ReMarker *m = &candidate->markers[candidate->marker_count];
            size_t base = candidate->format_version == 1 ? 2u : 4u;
            if ((candidate->format_version >= 2 &&
                 (strlen(tokens[1]) >= sizeof(m->id) || strlen(tokens[2]) >= sizeof(m->kind) ||
                  strlen(tokens[3]) >= sizeof(m->definition))) ||
                !integer(tokens[base], &m->sector) || !number(tokens[base + 1], &m->position.x) ||
                !number(tokens[base + 2], &m->position.y) ||
                !number(tokens[base + 3], &m->position.z) || !number(tokens[base + 4], &m->yaw)) {
                ok = fail(err, line_number, "Marcador invalido");
                break;
            }
            if (candidate->format_version == 1) {
                (void)snprintf(m->kind, sizeof(m->kind), "%s", tokens[1]);
                (void)snprintf(m->id, sizeof(m->id), "%s-%zu", tokens[1], candidate->marker_count);
                (void)snprintf(m->definition, sizeof(m->definition), "%s", tokens[1]);
            } else {
                (void)snprintf(m->id, sizeof(m->id), "%s", tokens[1]);
                (void)snprintf(m->kind, sizeof(m->kind), "%s", tokens[2]);
                (void)snprintf(m->definition, sizeof(m->definition), "%s", tokens[3]);
            }
            m->yaw *= RE_PI / 180.0f;
            m->source_line = line_number;
            candidate->marker_count++;
        } else if (candidate->format_version >= 2 && count == 9 &&
                   strcmp(tokens[0], "barrier") == 0) {
            if (candidate->barrier_count == RE_MAX_BARRIERS || strlen(tokens[1]) >= 32) {
                ok = fail(err, line_number, "Demasiadas barreras o ID demasiado largo");
                break;
            }
            ReBarrier *b = &candidate->barriers[candidate->barrier_count];
            int blocks = 0;
            if ((strcmp(tokens[2], "door") && strcmp(tokens[2], "window")) ||
                !integer(tokens[3], &b->sector) || !integer(tokens[4], &b->edge) ||
                !integer(tokens[5], &b->material) || !integer(tokens[6], &blocks) || blocks < 0 ||
                blocks > 7 || !number(tokens[7], &b->health) ||
                !number(tokens[8], &b->open_fraction)) {
                ok = fail(err, line_number, "Barrera invalida");
                break;
            }
            (void)snprintf(b->id, sizeof(b->id), "%s", tokens[1]);
            b->kind = strcmp(tokens[2], "door") == 0 ? RE_BARRIER_DOOR : RE_BARRIER_WINDOW;
            b->blocks = (unsigned int)blocks;
            b->source_line = line_number;
            candidate->barrier_count++;
        } else {
            ok =
                fail(err, line_number, "Directiva desconocida o cantidad de argumentos incorrecta");
            break;
        }
    }
    if (ferror(file))
        ok = fail(err, line_number, "Error leyendo el archivo");
    if (fclose(file) != 0)
        ok = fail(err, line_number, "Error cerrando el archivo");
    if (ok && !header)
        ok = fail(err, 1, "Mapa vacio");
    if (ok)
        ok = re_world_validate(candidate, err);
    if (ok) {
        *out = *candidate;
        *err = (ReError){0};
    }
    free(candidate);
    return ok;
}

bool re_sector_contains(const ReSector *s, ReVec2 point) {
    for (size_t i = 0; i < s->count; i++) {
        ReVec2 a = s->vertices[i], b = s->vertices[(i + 1) % s->count];
        if (re_cross2(re_sub2(b, a), re_sub2(point, a)) < -RE_EPSILON)
            return false;
    }
    return s->count >= 3;
}

/* SAT: dos convexos no se solapan si existe una normal de arista que los separa.
 * Contacto sobre una frontera es válido; intersección de interiores no lo es. */
static bool separating_axis(const ReSector *a, const ReSector *b) {
    for (size_t i = 0; i < a->count; i++) {
        ReVec2 edge = re_sub2(a->vertices[(i + 1) % a->count], a->vertices[i]);
        ReVec2 axis = re_normalize2(re_v2(-edge.y, edge.x));
        float amin = INFINITY, amax = -INFINITY, bmin = INFINITY, bmax = -INFINITY;
        for (size_t j = 0; j < a->count; j++) {
            float p = re_dot2(a->vertices[j], axis);
            amin = fminf(amin, p);
            amax = fmaxf(amax, p);
        }
        for (size_t j = 0; j < b->count; j++) {
            float p = re_dot2(b->vertices[j], axis);
            bmin = fminf(bmin, p);
            bmax = fmaxf(bmax, p);
        }
        if (amax <= bmin + RE_EPSILON || bmax <= amin + RE_EPSILON)
            return true;
    }
    return false;
}

bool re_world_validate(const ReWorld *w, ReError *err) {
    if (w->sector_count == 0 || w->sector_count > RE_MAX_SECTORS ||
        w->marker_count > RE_MAX_MARKERS || w->barrier_count > RE_MAX_BARRIERS)
        return fail(err, 0, "Cantidad de sectores o marcadores invalida");
    /* Primera pasada: validar tamaños antes de acceder a sectores vecinos. */
    for (size_t si = 0; si < w->sector_count; si++) {
        const ReSector *s = &w->sectors[si];
        if (s->count < 3 || s->count > RE_MAX_VERTICES || !isfinite(s->floor) ||
            !isfinite(s->ceiling) || s->ceiling - s->floor < 0.1f || !isfinite(s->light) ||
            s->light < 0 || s->light > 1)
            return fail(err, s->source_line, "Alturas, iluminacion o numero de vertices invalidos");
        int materials[3] = {s->wall_material, s->floor_material, s->ceiling_material};
        for (int i = 0; i < 3; i++)
            if (materials[i] < 0 || materials[i] >= RE_MAX_MATERIALS)
                return fail(err, s->source_line, "Material fuera de rango 0..15");
        for (size_t i = 0; i < s->count; i++) {
            ReVec2 a = s->vertices[i], b = s->vertices[(i + 1) % s->count];
            if (!isfinite(a.x) || !isfinite(a.y) || fabsf(a.x) > 10000 || fabsf(a.y) > 10000 ||
                re_length2(re_sub2(b, a)) < 0.01f)
                return fail(err, s->source_line, "Vertices no finitos o arista degenerada");
            for (size_t j = 0; j < s->count; j++)
                if (j != i && j != (i + 1) % s->count &&
                    re_cross2(re_sub2(b, a), re_sub2(s->vertices[j], a)) <= RE_EPSILON)
                    return fail(err, s->source_line,
                                "Poligono debe ser estrictamente convexo y antihorario");
        }
    }
    for (size_t si = 0; si < w->sector_count; si++) {
        const ReSector *s = &w->sectors[si];
        for (size_t i = 0; i < s->count; i++) {
            int n = s->neighbor[i], e = s->neighbor_edge[i];
            if (n == -1 && e == -1)
                continue;
            if (n < 0 || n >= (int)w->sector_count || n == (int)si || e < 0 ||
                e >= (int)w->sectors[n].count)
                return fail(err, s->source_line, "Indice de conexion invalido");
            const ReSector *other = &w->sectors[n];
            if (other->neighbor[e] != (int)si || other->neighbor_edge[e] != (int)i ||
                re_length2(re_sub2(s->vertices[i],
                                   other->vertices[((size_t)e + 1) % other->count])) > RE_EPSILON ||
                re_length2(re_sub2(s->vertices[(i + 1) % s->count], other->vertices[e])) >
                    RE_EPSILON)
                return fail(err, s->source_line, "Conexion no reciproca o extremos distintos");
            float start = s->portal_start[i], end = s->portal_end[i];
            float reciprocal_start = other->portal_start[e];
            float reciprocal_end = other->portal_end[e];
            if (!isfinite(start) || !isfinite(end) || start < 0 || end > 1 || end - start < 0.01f ||
                fabsf(reciprocal_start - (1 - end)) > RE_EPSILON ||
                fabsf(reciprocal_end - (1 - start)) > RE_EPSILON)
                return fail(err, s->source_line,
                            "Abertura invalida o distinta en la conexion reciproca");
        }
        for (size_t j = si + 1; j < w->sector_count; j++) {
            const ReSector *other = &w->sectors[j];
            bool separated_z =
                s->ceiling <= other->floor + RE_EPSILON || other->ceiling <= s->floor + RE_EPSILON;
            if (!separated_z && !separating_axis(s, other) && !separating_axis(other, s))
                return fail(err, s->source_line, "Sectores superpuestos");
        }
    }
    for (size_t i = 0; i < w->marker_count; i++) {
        const ReMarker *m = &w->markers[i];
        if (m->sector < 0 || m->sector >= (int)w->sector_count || !isfinite(m->position.x) ||
            !isfinite(m->position.y) || !isfinite(m->position.z) || !isfinite(m->yaw))
            return fail(err, m->source_line, "Sector o coordenadas del marcador invalidos");
        const ReSector *s = &w->sectors[m->sector];
        if (m->id[0] == '\0' || m->definition[0] == '\0' ||
            !re_sector_contains(s, re_v2(m->position.x, m->position.y)) ||
            m->position.z < s->floor || m->position.z >= s->ceiling)
            return fail(err, m->source_line, "Marcador fuera del volumen de su sector");
    }
    for (size_t i = 0; i < w->barrier_count; i++) {
        const ReBarrier *b = &w->barriers[i];
        if (b->id[0] == '\0' || b->sector < 0 || b->sector >= (int)w->sector_count || b->edge < 0 ||
            b->edge >= (int)w->sectors[b->sector].count ||
            w->sectors[b->sector].neighbor[b->edge] < 0 || b->material < 0 ||
            b->material >= RE_MAX_MATERIALS || b->blocks > 7u || !isfinite(b->health) ||
            b->health < 0 || !isfinite(b->open_fraction) || b->open_fraction < 0 ||
            b->open_fraction > 1)
            return fail(err, b->source_line, "Barrera fuera de rango o sin portal asociado");
        for (size_t j = 0; j < i; j++)
            if (strcmp(b->id, w->barriers[j].id) == 0 ||
                (b->sector == w->barriers[j].sector && b->edge == w->barriers[j].edge))
                return fail(err, b->source_line, "ID o arista de barrera duplicados");
    }
    return true;
}

static bool save_world(const char *path, const ReWorld *world, unsigned int version, ReError *err) {
    if (!path || !world || !err)
        return false;
    if (!re_world_validate(world, err))
        return false;
    char temporary[1024];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", path) < 0 ||
        strlen(path) + 4u >= sizeof(temporary))
        return fail(err, 0, "Ruta demasiado larga");
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return fail(err, 0, "No se pudo crear el mapa temporal");
    bool ok = fprintf(file, "retro_map %u\n", version) > 0;
    for (size_t si = 0; ok && si < world->sector_count; si++) {
        const ReSector *s = &world->sectors[si];
        ok = fprintf(file, "\nsector %zu %.9g %.9g %.9g %d %d %d\n", si, (double)s->floor,
                     (double)s->ceiling, (double)s->light, s->wall_material, s->floor_material,
                     s->ceiling_material) > 0;
        for (size_t v = 0; ok && v < s->count; v++)
            ok = fprintf(file, "v %.9g %.9g\n", (double)s->vertices[v].x,
                         (double)s->vertices[v].y) > 0;
        for (size_t e = 0; ok && e < s->count; e++)
            if (s->neighbor[e] >= 0) {
                if (version >= 4)
                    ok = fprintf(file, "link %zu %d %d %.9g %.9g\n", e, s->neighbor[e],
                                 s->neighbor_edge[e], (double)s->portal_start[e],
                                 (double)s->portal_end[e]) > 0;
                else
                    ok = fprintf(file, "link %zu %d %d\n", e, s->neighbor[e], s->neighbor_edge[e]) >
                         0;
            }
    }
    for (size_t i = 0; ok && i < world->marker_count; i++) {
        const ReMarker *m = &world->markers[i];
        ok = fprintf(file, "\nspawn %s %s %s %d %.9g %.9g %.9g %.9g\n", m->id, m->kind,
                     m->definition, m->sector, (double)m->position.x, (double)m->position.y,
                     (double)m->position.z, (double)(m->yaw * 180.0f / RE_PI)) > 0;
    }
    for (size_t i = 0; ok && i < world->barrier_count; i++) {
        const ReBarrier *b = &world->barriers[i];
        ok = fprintf(file, "barrier %s %s %d %d %d %u %.9g %.9g\n", b->id,
                     b->kind == RE_BARRIER_DOOR ? "door" : "window", b->sector, b->edge,
                     b->material, b->blocks, (double)b->health, (double)b->open_fraction) > 0;
    }
    if (fclose(file) != 0)
        ok = false;
    if (!ok) {
        (void)remove(temporary);
        return fail(err, 0, "Error escribiendo el mapa temporal");
    }
    if (!replace_file(temporary, path)) {
        (void)remove(temporary);
        return fail(err, 0, "No se pudo reemplazar el mapa guardado");
    }
    *err = (ReError){0};
    return true;
}

bool re_world_save_v2(const char *path, const ReWorld *world, ReError *err) {
    return save_world(path, world, 2, err);
}

bool re_world_save_v3(const char *path, const ReWorld *world, ReError *err) {
    return save_world(path, world, 3, err);
}

bool re_world_save_v4(const char *path, const ReWorld *world, ReError *err) {
    return save_world(path, world, 4, err);
}
