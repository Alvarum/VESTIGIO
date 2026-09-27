#ifndef VESTIGIO_PLAYER_DEMO_3D_H
#define VESTIGIO_PLAYER_DEMO_3D_H

#include "vestigio/vestigio.h"
#include <stdbool.h>

int vg_demo_3d_run(int smoke_frames, const char *capture_path, bool show_colliders,
                   const char *level_path, bool smoke_door,
                   bool audio_enabled, bool save_audio_overrides,
                   const char *settings_path,
                   const VgSettingsLayer *session_settings);

#endif
