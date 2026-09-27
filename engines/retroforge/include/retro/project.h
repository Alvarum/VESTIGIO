/* Proyecto editable: manifiesto, mapa y definiciones cargadas como una unidad.
 * Las rutas guardadas son relativas al manifiesto, por lo que un proyecto se
 * puede mover, comprimir o abrir desde una ruta con espacios. */
#ifndef RETRO_PROJECT_H
#define RETRO_PROJECT_H

#include "retro/interaction.h"

enum { RE_PROJECT_PATH = 512 };
enum ReDeathPolicy {
    RE_DEATH_RESTART_LEVEL,
    RE_DEATH_LAST_CHECKPOINT,
    RE_DEATH_LIMITED_LIVES,
    RE_DEATH_PERMADEATH
};
typedef struct ReProject {
    char manifest[RE_PROJECT_PATH];
    char root[RE_PROJECT_PATH];
    char id[64];
    char name[64];
    char rules[32];
    char initial_level[RE_PROJECT_PATH];
    /* Arte opcional de portada. La plataforma lo carga; el formato del
     * proyecto sólo conserva una ruta relativa y sigue libre de raylib. */
    char title_art[RE_PROJECT_PATH];
    /* Una ruta opcional por índice de material. El índice coincide con el
     * usado por el mapa; una entrada vacía conserva el patrón de respaldo. */
    char material_files[RE_MAX_MATERIALS][RE_PROJECT_PATH];
    char logic_file[RE_PROJECT_PATH];
    char dialogue_file[RE_PROJECT_PATH];
    char actor_files[RE_MAX_CHARACTER_DEFS][RE_PROJECT_PATH];
    ReWorld world;
    ReCharacterDef characters[RE_MAX_CHARACTER_DEFS];
    size_t character_count;
    ReInteractionDefinitions interactions;
    enum ReDeathPolicy death_policy;
    unsigned int format_version;
} ReProject;

[[nodiscard]] bool re_project_load(const char *manifest, ReProject *out, ReError *error);
[[nodiscard]] bool re_project_save(const ReProject *project, ReError *error);
/* Copia binaria con límite de 64 MiB. relative_dest no puede escapar de root. */
[[nodiscard]] bool re_project_import(const ReProject *project, const char *source,
                                     const char *relative_dest, ReError *error);
[[nodiscard]] bool re_project_path(const ReProject *project, const char *relative, char *out,
                                   size_t capacity);

#endif
