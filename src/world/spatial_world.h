#ifndef VESTIGIO_WORLD_SPATIAL_WORLD_H
#define VESTIGIO_WORLD_SPATIAL_WORLD_H

#include "physics/spatial.h"

/* Refreshes the collider from the authoritative runtime world Transform. The
 * collider description remains the common shape/layer source for all queries. */
VgResult vg_spatial_world_refresh_entity(VgContext *context, VgSpatialScene *scene,
                                         VgSpatialCollider collider,
                                         const VgSpatialColliderDesc *description);

#endif
