/* Persistencia del proyecto. Se usan formatos de texto pequeños y explícitos
 * para que Git muestre cambios útiles y una persona pueda reparar un archivo. */
#include "retro/project.h"
#include "retro/transaction.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

static bool fail(ReError *error, size_t line, const char *message) {
    error->line = line;
    (void)snprintf(error->message, sizeof(error->message), "%s", message);
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

static bool unsafe_relative(const char *path) {
    if (!path || !*path || path[0] == '/' || path[0] == '\\' ||
        (isalpha((unsigned char)path[0]) && path[1] == ':'))
        return true;
    const char *p = path;
    while ((p = strstr(p, "..")) != nullptr) {
        bool left = p == path || p[-1] == '/' || p[-1] == '\\';
        bool right = p[2] == '\0' || p[2] == '/' || p[2] == '\\';
        if (left && right)
            return true;
        p += 2;
    }
    return false;
}

bool re_project_path(const ReProject *project, const char *relative, char *out, size_t capacity) {
    if (!project || !out || !capacity || unsafe_relative(relative))
        return false;
    int written = snprintf(out, capacity, "%s/%s", project->root, relative);
    if (written < 0 || (size_t)written >= capacity)
        return false;
    for (char *p = out; *p; p++)
        if (*p == '\\')
            *p = '/';
    return true;
}

static void root_from_manifest(const char *manifest, char *root, size_t capacity) {
    (void)snprintf(root, capacity, "%s", manifest);
    char *slash = strrchr(root, '/');
    char *backslash = strrchr(root, '\\');
    if (!slash || (backslash && backslash > slash))
        slash = backslash;
    if (slash)
        *slash = '\0';
    else
        (void)snprintf(root, capacity, ".");
}

static size_t split(char *line, char **parts, size_t capacity) {
    size_t count = 0;
    for (char *p = line; *p;) {
        while (isspace((unsigned char)*p))
            p++;
        if (!*p || *p == '#')
            break;
        if (count == capacity)
            return capacity + 1u;
        parts[count++] = p;
        while (*p && !isspace((unsigned char)*p) && *p != '#')
            p++;
        if (*p == '#') {
            *p = '\0';
            break;
        }
        if (*p)
            *p++ = '\0';
    }
    return count;
}

bool re_project_load(const char *manifest, ReProject *out, ReError *error) {
    if (!re_project_recover(manifest, error))
        return false;
    if (!manifest || !out || !error)
        return false;
    FILE *file = fopen(manifest, "rb");
    if (!file)
        return fail(error, 0, "No se pudo abrir el proyecto");
    ReProject *candidate = calloc(1, sizeof(*candidate));
    if (!candidate) {
        (void)fclose(file);
        return fail(error, 0, "Memoria insuficiente para cargar el proyecto");
    }
    (void)snprintf(candidate->manifest, sizeof(candidate->manifest), "%s", manifest);
    root_from_manifest(manifest, candidate->root, sizeof(candidate->root));
    (void)snprintf(candidate->rules, sizeof(candidate->rules), "fps");
    candidate->death_policy = RE_DEATH_LAST_CHECKPOINT;
    bool header = false, ok = true;
    unsigned int version = 0;
    size_t line_number = 0;
    char line[1024];
    while (ok && fgets(line, sizeof(line), file)) {
        line_number++;
        char *part[4];
        size_t count = split(line, part, 4);
        if (!count)
            continue;
        if (!header) {
            header = count == 2 && strcmp(part[0], "retro_project") == 0 &&
                     (strcmp(part[1], "1") == 0 || strcmp(part[1], "2") == 0);
            version = header ? (unsigned int)(part[1][0] - '0') : 0;
            if (!header)
                ok = fail(error, line_number, "Se esperaba retro_project 1 o 2");
        } else if (count == 2 && strcmp(part[0], "id") == 0 && strlen(part[1]) < 64) {
            (void)snprintf(candidate->id, sizeof(candidate->id), "%s", part[1]);
        } else if (count == 2 && strcmp(part[0], "name") == 0 && strlen(part[1]) < 64) {
            (void)snprintf(candidate->name, sizeof(candidate->name), "%s", part[1]);
        } else if (count == 2 && strcmp(part[0], "rules") == 0 && strlen(part[1]) < 32) {
            (void)snprintf(candidate->rules, sizeof(candidate->rules), "%s", part[1]);
        } else if (count == 2 && strcmp(part[0], "death") == 0) {
            if (strcmp(part[1], "restart_level") == 0)
                candidate->death_policy = RE_DEATH_RESTART_LEVEL;
            else if (strcmp(part[1], "checkpoint") == 0)
                candidate->death_policy = RE_DEATH_LAST_CHECKPOINT;
            else if (strcmp(part[1], "limited_lives") == 0)
                candidate->death_policy = RE_DEATH_LIMITED_LIVES;
            else if (strcmp(part[1], "permadeath") == 0)
                candidate->death_policy = RE_DEATH_PERMADEATH;
            else
                ok = fail(error, line_number, "Política death inválida");
        } else if (count == 2 && strcmp(part[0], "level") == 0 &&
                   strlen(part[1]) < RE_PROJECT_PATH && !unsafe_relative(part[1])) {
            (void)snprintf(candidate->initial_level, sizeof(candidate->initial_level), "%s",
                           part[1]);
        } else if (count == 2 && strcmp(part[0], "title_art") == 0 &&
                   strlen(part[1]) < RE_PROJECT_PATH && !unsafe_relative(part[1])) {
            (void)snprintf(candidate->title_art, sizeof(candidate->title_art), "%s", part[1]);
        } else if (count == 2 && strcmp(part[0], "logic") == 0 &&
                   strlen(part[1]) < RE_PROJECT_PATH && !unsafe_relative(part[1])) {
            (void)snprintf(candidate->logic_file, sizeof(candidate->logic_file), "%s", part[1]);
        } else if (count == 2 && strcmp(part[0], "dialogue") == 0 &&
                   strlen(part[1]) < RE_PROJECT_PATH && !unsafe_relative(part[1])) {
            (void)snprintf(candidate->dialogue_file, sizeof(candidate->dialogue_file), "%s",
                           part[1]);
        } else if (count == 2 && strcmp(part[0], "actor") == 0 &&
                   candidate->character_count < RE_MAX_CHARACTER_DEFS &&
                   strlen(part[1]) < RE_PROJECT_PATH && !unsafe_relative(part[1])) {
            (void)snprintf(candidate->actor_files[candidate->character_count], RE_PROJECT_PATH,
                           "%s", part[1]);
            candidate->character_count++;
        } else
            ok = fail(error, line_number, "Directiva de proyecto desconocida o inválida");
    }
    bool read_error = ferror(file) != 0;
    int close_result = fclose(file);
    if (read_error || close_result != 0)
        ok = fail(error, line_number, "Error leyendo el proyecto");
    candidate->format_version = version;
    if (ok && (!header || !candidate->name[0] || !candidate->initial_level[0]))
        ok = fail(error, 0, "El proyecto necesita name y level");
    if (ok && !candidate->id[0])
        (void)snprintf(candidate->id, sizeof(candidate->id), "%s", candidate->name);
    for (const char *p = candidate->id; ok && *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_')
            ok = fail(error, 0, "project id sólo admite letras ASCII, números, - y _");
    char path[RE_PROJECT_PATH * 2];
    if (ok && (!re_project_path(candidate, candidate->initial_level, path, sizeof(path)) ||
               !re_world_load(path, &candidate->world, error)))
        ok = false;
    for (size_t i = 0; ok && i < candidate->character_count; i++) {
        if (!re_project_path(candidate, candidate->actor_files[i], path, sizeof(path)) ||
            !re_character_load(path, &candidate->characters[i], error))
            ok = false;
        for (size_t j = 0; ok && j < i; j++)
            if (strcmp(candidate->characters[i].id, candidate->characters[j].id) == 0)
                ok = fail(error, 0, "ID de personaje duplicado");
    }
    if (ok && candidate->logic_file[0] &&
        (!re_project_path(candidate, candidate->logic_file, path, sizeof(path)) ||
         !re_interaction_load_rules(path, &candidate->interactions, error)))
        ok = false;
    if (ok && candidate->dialogue_file[0] &&
        (!re_project_path(candidate, candidate->dialogue_file, path, sizeof(path)) ||
         !re_interaction_load_dialogues(path, &candidate->interactions, error)))
        ok = false;
    if (ok) {
        *out = *candidate;
        *error = (ReError){0};
    }
    free(candidate);
    return ok;
}

bool re_project_write_manifest(const char *path, const ReProject *project, ReError *error) {
    if (!project || !error || !path || !path[0])
        return false;
    char temporary[RE_PROJECT_PATH * 2 + 40];
    int length = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (length < 0 || (size_t)length >= sizeof(temporary))
        return fail(error, 0, "Ruta de proyecto demasiado larga");
    FILE *file = fopen(temporary, "wb");
    if (!file)
        return fail(error, 0, "No se pudo crear el manifiesto temporal");
    static const char *const death_names[] = {"restart_level", "checkpoint", "limited_lives",
                                              "permadeath"};
    enum ReDeathPolicy death = project->death_policy;
    if (death < RE_DEATH_RESTART_LEVEL || death > RE_DEATH_PERMADEATH)
        death = RE_DEATH_LAST_CHECKPOINT;
    bool ok = fprintf(file, "retro_project 2\nid %s\nname %s\nrules %s\ndeath %s\nlevel %s\n",
                      project->id, project->name, project->rules, death_names[death],
                      project->initial_level) > 0;
    if (ok && project->logic_file[0])
        ok = fprintf(file, "logic %s\n", project->logic_file) > 0;
    if (ok && project->dialogue_file[0])
        ok = fprintf(file, "dialogue %s\n", project->dialogue_file) > 0;
    if (ok && project->title_art[0])
        ok = fprintf(file, "title_art %s\n", project->title_art) > 0;
    for (size_t i = 0; ok && i < project->character_count; i++)
        ok = fprintf(file, "actor %s\n", project->actor_files[i]) > 0;
    if (fclose(file) != 0)
        ok = false;
    if (!ok) {
        (void)remove(temporary);
        return error->message[0] ? false : fail(error, 0, "Error guardando el proyecto");
    }
    if (!replace_file(temporary, path)) {
        (void)remove(temporary);
        return fail(error, 0, "No se pudo reemplazar el manifiesto");
    }
    *error = (ReError){0};
    return true;
}

bool re_project_save(const ReProject *project, ReError *error) {
    return re_project_commit(project, error);
}

bool re_project_import(const ReProject *project, const char *source, const char *relative,
                       ReError *error) {
    char destination[RE_PROJECT_PATH * 2];
    if (!project || !source || !relative || !error ||
        !re_project_path(project, relative, destination, sizeof(destination)))
        return fail(error, 0, "Destino de recurso inseguro o demasiado largo");
    FILE *input = fopen(source, "rb");
    if (!input)
        return fail(error, 0, "No se pudo abrir el recurso de origen");
    FILE *output = fopen(destination, "wb");
    if (!output) {
        (void)fclose(input);
        return fail(error, 0, "No se pudo crear el recurso importado");
    }
    unsigned char buffer[16384];
    size_t total = 0;
    bool ok = true;
    while (ok) {
        size_t read = fread(buffer, 1, sizeof(buffer), input);
        if (!read)
            break;
        total += read;
        ok = total <= 64u * 1024u * 1024u && fwrite(buffer, 1, read, output) == read;
    }
    bool read_error = ferror(input) != 0;
    int input_close = fclose(input);
    int output_close = fclose(output);
    if (read_error || input_close != 0 || output_close != 0)
        ok = false;
    if (!ok) {
        (void)remove(destination);
        return fail(error, 0, "Recurso mayor que 64 MiB o error copiándolo");
    }
    *error = (ReError){0};
    return true;
}
