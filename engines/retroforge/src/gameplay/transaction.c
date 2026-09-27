/* Transacción de autoría recuperable. Primero se escriben TODOS los candidatos;
 * después se conservan las versiones anteriores y se publica el diario.
 * Mientras exista el diario, recuperar significa volver al conjunto anterior.
 * No se borran backups hasta retirar el diario: repetir recuperación es seguro. */
#include "retro/transaction.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#endif

enum { TX_FILES = RE_MAX_CHARACTER_DEFS + 4, TX_PATH = RE_PROJECT_PATH * 2 + 32 };
typedef struct Entry {
    char path[TX_PATH];
    int kind;
    size_t index;
    bool existed;
} Entry;

static bool fail(ReError *error, const char *message) {
    if (error) {
        error->line = 0;
        (void)snprintf(error->message, sizeof(error->message), "%s", message);
    }
    return false;
}
static bool suffix(const char *path, const char *tail, char *out) {
    int size = snprintf(out, TX_PATH, "%s%s", path, tail);
    return size >= 0 && size < TX_PATH;
}
static bool same_path(const char *left, const char *right) {
#ifdef _WIN32
    /* Windows ignora mayúsculas y normaliza segmentos '.'. Dos recursos no
     * pueden publicar candidatos distintos sobre el mismo archivo físico. */
    char a[TX_PATH], b[TX_PATH];
    DWORD na = GetFullPathNameA(left, TX_PATH, a, nullptr);
    DWORD nb = GetFullPathNameA(right, TX_PATH, b, nullptr);
    return na > 0 && na < TX_PATH && nb > 0 && nb < TX_PATH && lstrcmpiA(a, b) == 0;
#else
    return strcmp(left, right) == 0;
#endif
}
static bool replace(const char *source, const char *destination) {
#ifdef _WIN32
    return MoveFileExA(source, destination, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
           0;
#else
    return rename(source, destination) == 0;
#endif
}
static bool durable_close(FILE *file) {
    bool ok = fflush(file) == 0;
#ifdef _WIN32
    if (ok)
        ok = _commit(_fileno(file)) == 0;
#endif
    return fclose(file) == 0 && ok;
}
static bool copy_file(const char *source, const char *destination) {
    FILE *in = fopen(source, "rb");
    if (!in)
        return false;
    FILE *out = fopen(destination, "wb");
    if (!out) {
        (void)fclose(in);
        return false;
    }
    unsigned char buffer[16384];
    size_t size, total = 0;
    bool ok = true;
    while ((size = fread(buffer, 1, sizeof(buffer), in)) != 0) {
        total += size;
        if (total > 64u * 1024u * 1024u || fwrite(buffer, 1, size, out) != size) {
            ok = false;
            break;
        }
    }
    if (ferror(in))
        ok = false;
    if (fclose(in) != 0)
        ok = false;
    return durable_close(out) && ok;
}
static void root(const char *manifest, ReProject *project) {
    (void)snprintf(project->root, sizeof(project->root), "%s", manifest);
    char *a = strrchr(project->root, '/'), *b = strrchr(project->root, '\\');
    if (b && (!a || b > a))
        a = b;
    if (a)
        *a = '\0';
    else
        (void)snprintf(project->root, sizeof(project->root), ".");
}

bool re_project_recover(const char *manifest, ReError *error) {
    if (!manifest || !error)
        return false;
    char journal[TX_PATH];
    if (!suffix(manifest, ".rf-journal", journal))
        return fail(error, "Ruta demasiado larga");
    FILE *file = fopen(journal, "rb");
    if (!file)
        return errno == ENOENT ? true : fail(error, "No se pudo leer el diario de recuperación");
    Entry *entries = calloc(TX_FILES, sizeof(*entries));
    ReProject *paths = calloc(1, sizeof(*paths));
    if (!entries || !paths) {
        free(entries);
        free(paths);
        (void)fclose(file);
        return fail(error, "Memoria insuficiente");
    }
    root(manifest, paths);
    char line[TX_PATH];
    bool ok =
        fgets(line, sizeof(line), file) != nullptr && strcmp(line, "retro_transaction 1\n") == 0;
    size_t count = 0;
    while (ok && fgets(line, sizeof(line), file)) {
        size_t length = strlen(line);
        if (count >= TX_FILES || length < 4 || line[length - 1] != '\n' ||
            (line[0] != '0' && line[0] != '1') || line[1] != ' ') {
            ok = false;
            break;
        }
        line[length - 1] = '\0';
        entries[count].existed = line[0] == '1';
        ok = re_project_path(paths, line + 2, entries[count].path, sizeof(entries[count].path));
        for (size_t j = 0; ok && j < count; j++)
            if (same_path(entries[j].path, entries[count].path))
                ok = false;
        count++;
    }
    if (ferror(file))
        ok = false;
    if (fclose(file) != 0 || count == 0)
        ok = false;
    for (size_t i = 0; ok && i < count; i++) {
        char backup[TX_PATH], temporary[TX_PATH];
        ok = suffix(entries[i].path, ".rf-old", backup) &&
             suffix(entries[i].path, ".rf-recover", temporary);
        if (!ok)
            break;
        if (entries[i].existed)
            ok = copy_file(backup, temporary) && replace(temporary, entries[i].path);
        else if (remove(entries[i].path) != 0 && errno != ENOENT)
            ok = false;
    }
    if (ok)
        ok = remove(journal) == 0;
    if (ok)
        for (size_t i = 0; i < count; i++) {
            char path[TX_PATH];
            if (suffix(entries[i].path, ".rf-old", path))
                (void)remove(path);
            if (suffix(entries[i].path, ".rf-new", path))
                (void)remove(path);
        }
    free(entries);
    free(paths);
    return ok ? true : fail(error, "Recuperación incompleta: se conservó el diario y los backups");
}

bool re_project_commit(const ReProject *project, ReError *error) {
    if (!project || !error || !re_project_recover(project->manifest, error))
        return false;
    if (project->character_count > RE_MAX_CHARACTER_DEFS)
        return fail(error, "El proyecto excede la capacidad de personajes");
    if (!re_world_validate(&project->world, error))
        return false;
    Entry *entries = calloc(TX_FILES, sizeof(*entries));
    if (!entries)
        return fail(error, "Memoria insuficiente");
    size_t count = 0;
    const char *files[3] = {project->initial_level, project->logic_file, project->dialogue_file};
    bool ok = true;
    for (size_t i = 0; i < 3; i++)
        if (files[i][0]) {
            entries[count].kind = (int)i;
            ok = ok && re_project_path(project, files[i], entries[count].path, TX_PATH);
            count++;
        }
    for (size_t i = 0; i < project->character_count && ok; i++) {
        if (count >= TX_FILES - 1) {
            ok = false;
            break;
        }
        entries[count].kind = 3;
        entries[count].index = i;
        ok = re_character_validate(&project->characters[i], error) &&
             re_project_path(project, project->actor_files[i], entries[count].path, TX_PATH);
        count++;
    }
    entries[count].kind = 4;
    const char *name = strrchr(project->manifest, '/'), *back = strrchr(project->manifest, '\\');
    if (back && (!name || back > name))
        name = back;
    name = name ? name + 1 : project->manifest;
    ok = ok && re_project_path(project, name, entries[count].path, TX_PATH);
    count++;
    for (size_t i = 0; i < count && ok; i++) {
        for (size_t j = 0; j < i; j++)
            if (same_path(entries[i].path, entries[j].path))
                ok = false;
    }
    /* Preparación: ningún original cambia si falla un serializador. */
    for (size_t i = 0; i < count && ok; i++) {
        char staged[TX_PATH];
        ok = suffix(entries[i].path, ".rf-new", staged);
        if (!ok)
            break;
        switch (entries[i].kind) {
        case 0:
            ok = re_world_save_v4(staged, &project->world, error);
            break;
        case 1:
            ok = re_interaction_save_rules(staged, &project->interactions, error);
            break;
        case 2:
            ok = re_interaction_save_dialogues(staged, &project->interactions, error);
            break;
        case 3:
            ok = re_character_save(staged, &project->characters[entries[i].index], error);
            break;
        default:
            ok = re_project_write_manifest(staged, project, error);
            break;
        }
    }
    for (size_t i = 0; i < count && ok; i++) {
        FILE *existing = fopen(entries[i].path, "rb");
        if (!existing) {
            if (errno != ENOENT)
                ok = false;
            continue;
        }
        entries[i].existed = true;
        ok = fclose(existing) == 0;
        char backup[TX_PATH];
        ok = ok && suffix(entries[i].path, ".rf-old", backup) && copy_file(entries[i].path, backup);
    }
    char journal[TX_PATH], journal_new[TX_PATH];
    ok = ok && suffix(project->manifest, ".rf-journal", journal) &&
         suffix(project->manifest, ".rf-journal.tmp", journal_new);
    bool published = false;
    if (ok) {
        FILE *file = fopen(journal_new, "wb");
        ok = file != nullptr;
        if (file) {
            ok = fprintf(file, "retro_transaction 1\n") > 0;
            size_t prefix = strlen(project->root) + 1u;
            for (size_t i = 0; i < count && ok; i++)
                ok = fprintf(file, "%d %s\n", entries[i].existed ? 1 : 0,
                             entries[i].path + prefix) > 0;
            ok = durable_close(file) && ok;
        }
        if (ok) {
            ok = replace(journal_new, journal);
            published = ok;
        }
    }
    for (size_t i = 0; i < count && ok; i++) {
        char staged[TX_PATH];
        ok = suffix(entries[i].path, ".rf-new", staged) && replace(staged, entries[i].path);
    }
    if (ok)
        ok = remove(journal) == 0;
    if (!ok && published) {
        ReError recovery = {0};
        if (!re_project_recover(project->manifest, &recovery)) {
            *error = recovery;
            free(entries);
            return false;
        }
    }
    for (size_t i = 0; i < count; i++) {
        char path[TX_PATH];
        if (suffix(entries[i].path, ".rf-old", path))
            (void)remove(path);
        if (suffix(entries[i].path, ".rf-new", path))
            (void)remove(path);
    }
    free(entries);
    if (!ok)
        return fail(error, "No se pudo guardar: el proyecto anterior se conserva");
    *error = (ReError){0};
    return true;
}
