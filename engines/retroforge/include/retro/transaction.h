/* Guardado coordinado del documento: archivos preparados, copia anterior y
 * diario de recuperación. Las partidas tienen un formato independiente. */
#ifndef RETRO_TRANSACTION_H
#define RETRO_TRANSACTION_H
#include "retro/project.h"
[[nodiscard]] bool re_project_commit(const ReProject *project, ReError *error);
[[nodiscard]] bool re_project_recover(const char *manifest, ReError *error);
/* Detalle compartido por el escritor transaccional; no guarda hijos. */
[[nodiscard]] bool re_project_write_manifest(const char *path, const ReProject *project,
                                             ReError *error);
#endif
