#include "vestigio/controller.h"

#include <math.h>
#include <stddef.h>

#define VG_CONTROLLER_SKIN 0.002f
#define VG_CONTROLLER_MAX_SAMPLES 64u
#define VG_CONTROLLER_SAMPLE_SPACING 0.25f

static VgVec3 controller_add(VgVec3 a, VgVec3 b) {
    return (VgVec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static VgVec3 controller_scale(VgVec3 a, float scale) {
    return (VgVec3){a.x * scale, a.y * scale, a.z * scale};
}

static float controller_dot(VgVec3 a, VgVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float controller_length(VgVec3 a) {
    return sqrtf(controller_dot(a, a));
}

static bool controller_finite_vec(VgVec3 a) {
    return isfinite(a.x) && isfinite(a.y) && isfinite(a.z);
}

static bool controller_valid_config(const VgControllerConfig *config) {
    return config != NULL && config->struct_size >= sizeof(*config) &&
           config->api_version == VG_API_VERSION && isfinite(config->radius) &&
           isfinite(config->height) && isfinite(config->eye_height) &&
           isfinite(config->step_height) && isfinite(config->ground_snap) &&
           isfinite(config->min_ground_normal_z) && isfinite(config->move_speed) &&
           isfinite(config->gravity) && isfinite(config->jump_speed) &&
           isfinite(config->max_fixed_dt) && config->radius > VG_CONTROLLER_SKIN &&
           config->height >= config->radius * 2.0f && config->height <= 4.0f &&
           config->height <= config->radius * (2.0f +
                              VG_CONTROLLER_SAMPLE_SPACING *
                                  (float)(VG_CONTROLLER_MAX_SAMPLES - 1u)) &&
           config->eye_height > 0.0f && config->eye_height <= config->height &&
           config->step_height >= 0.0f && config->step_height <= config->height * 0.5f &&
           config->ground_snap >= 0.0f && config->ground_snap <= config->height &&
           config->min_ground_normal_z > 0.0f && config->min_ground_normal_z <= 1.0f &&
           config->move_speed > 0.0f && config->move_speed <= 50.0f &&
           config->gravity >= 0.0f && config->gravity <= 100.0f &&
           config->jump_speed >= 0.0f && config->jump_speed <= 50.0f &&
           config->max_fixed_dt >= 1.0f / 240.0f &&
           config->max_fixed_dt <= 1.0f / 30.0f &&
           config->collision_mask != 0u;
}

VgControllerConfig vg_controller_default_config(void) {
    return (VgControllerConfig){sizeof(VgControllerConfig), VG_API_VERSION, 0.28f, 1.7f,
                                1.55f, 0.34f, 0.16f, 0.7071068f, 3.2f, 14.0f,
                                4.6f, 1.0f / 60.0f, UINT64_MAX};
}

static VgResult controller_sweep(VgSpatialScene *scene, const VgControllerConfig *config,
                                 VgEntity ignored_entity, VgVec3 feet, VgVec3 delta,
                                 bool *out_hit, VgSpatialHit *out_result) {
    float length = controller_length(delta);
    *out_hit = false;
    if (length < 1.0e-7f)
        return VG_OK;
    float centerline = config->height - 2.0f * config->radius;
    uint32_t segments = (uint32_t)ceilf(centerline /
                                         (config->radius * VG_CONTROLLER_SAMPLE_SPACING));
    if (segments == 0u)
        segments = 1u;
    if (segments >= VG_CONTROLLER_MAX_SAMPLES)
        segments = VG_CONTROLLER_MAX_SAMPLES - 1u;
    VgVec3 direction = controller_scale(delta, 1.0f / length);
    for (uint32_t index = 0u; index <= segments; ++index) {
        float fraction = (float)index / (float)segments;
        VgSpatialSphereSweep query = {0};
        query.center = feet;
        query.center.z += config->radius + centerline * fraction;
        query.radius = config->radius;
        query.direction = direction;
        query.max_distance = length;
        query.layer_mask = config->collision_mask;
        query.ignored_entity = ignored_entity;
        bool hit = false;
        VgSpatialHit result = {0};
        VgResult error = vg_spatial_sweep_sphere(scene, &query, &hit, &result);
        if (error != VG_OK)
            return error;
        if (hit && (!*out_hit || result.distance < out_result->distance)) {
            *out_hit = true;
            *out_result = result;
        }
    }
    return VG_OK;
}

static VgResult controller_overlap(VgSpatialScene *scene, const VgControllerConfig *config,
                                   VgEntity ignored_entity, VgVec3 feet, bool *out_overlap) {
    float centerline = config->height - 2.0f * config->radius;
    uint32_t segments = (uint32_t)ceilf(centerline /
                                         (config->radius * VG_CONTROLLER_SAMPLE_SPACING));
    if (segments == 0u)
        segments = 1u;
    if (segments >= VG_CONTROLLER_MAX_SAMPLES)
        segments = VG_CONTROLLER_MAX_SAMPLES - 1u;
    *out_overlap = false;
    for (uint32_t index = 0u; index <= segments; ++index) {
        VgSpatialSphereQuery query = {0};
        query.center = feet;
        query.center.z += config->radius + centerline * (float)index / (float)segments;
        query.radius = config->radius;
        query.layer_mask = config->collision_mask;
        query.ignored_entity = ignored_entity;
        bool hit = false;
        VgSpatialHit result = {0};
        VgResult error = vg_spatial_overlap_sphere(scene, &query, &hit, &result);
        if (error != VG_OK)
            return error;
        if (hit && result.distance < -VG_CONTROLLER_SKIN) {
            *out_overlap = true;
            return VG_OK;
        }
    }
    return VG_OK;
}

static VgResult controller_ground(VgSpatialScene *scene, const VgControllerConfig *config,
                                  VgEntity ignored_entity, VgVec3 feet, float distance,
                                  bool *out_ground, float *out_drop) {
    VgSpatialSphereSweep query = {0};
    query.center = feet;
    query.center.z += config->radius + VG_CONTROLLER_SKIN;
    query.radius = config->radius;
    query.direction = (VgVec3){0.0f, 0.0f, -1.0f};
    query.max_distance = distance + VG_CONTROLLER_SKIN;
    query.layer_mask = config->collision_mask;
    query.ignored_entity = ignored_entity;
    bool hit = false;
    VgSpatialHit result = {0};
    VgResult error = vg_spatial_sweep_sphere(scene, &query, &hit, &result);
    if (error != VG_OK)
        return error;
    *out_ground = hit && result.normal.z >= config->min_ground_normal_z &&
                  result.distance >= -VG_CONTROLLER_SKIN;
    *out_drop = *out_ground ? fmaxf(0.0f, result.distance - VG_CONTROLLER_SKIN) : 0.0f;
    return VG_OK;
}

static VgResult controller_try_step(VgSpatialScene *scene, const VgControllerConfig *config,
                                    VgEntity ignored_entity, VgVec3 feet, VgVec3 movement,
                                    bool *out_stepped, VgVec3 *out_feet) {
    *out_stepped = false;
    if (config->step_height <= 0.0f)
        return VG_OK;
    VgVec3 raise = {0.0f, 0.0f, config->step_height};
    bool hit = false;
    VgSpatialHit result = {0};
    VgResult error = controller_sweep(scene, config, ignored_entity, feet, raise, &hit, &result);
    if (error != VG_OK || hit)
        return error;
    VgVec3 candidate = controller_add(feet, raise);
    error = controller_sweep(scene, config, ignored_entity, candidate, movement, &hit, &result);
    if (error != VG_OK || hit)
        return error;
    candidate = controller_add(candidate, movement);
    bool ground = false;
    float drop = 0.0f;
    error = controller_ground(scene, config, ignored_entity, candidate,
                              config->step_height + config->ground_snap, &ground, &drop);
    if (error != VG_OK || !ground)
        return error;
    candidate.z -= drop;
    bool overlap = false;
    error = controller_overlap(scene, config, ignored_entity, candidate, &overlap);
    if (error != VG_OK || overlap)
        return error;
    *out_feet = candidate;
    *out_stepped = true;
    return VG_OK;
}

VgResult vg_controller_step(VgSpatialScene *scene, const VgControllerConfig *config,
                            VgEntity ignored_entity, const VgControllerInput *input,
                            float fixed_dt, VgControllerState *in_out_state) {
    if (scene == NULL || !controller_valid_config(config) || input == NULL ||
        in_out_state == NULL || !controller_finite_vec(input->move_world) ||
        !controller_finite_vec(in_out_state->feet) ||
        !isfinite(in_out_state->vertical_speed) ||
        !isfinite(in_out_state->last_ground_height) || !isfinite(fixed_dt) ||
        fixed_dt <= 0.0f || fixed_dt > config->max_fixed_dt + 1.0e-6f)
        return VG_ERROR_INVALID_ARGUMENT;
    VgControllerState state = *in_out_state;
    bool overlap = false;
    VgResult error = controller_overlap(scene, config, ignored_entity, state.feet, &overlap);
    if (error != VG_OK)
        return error;
    if (overlap)
        return VG_ERROR_CONFLICT; /* Spawn inside geometry: caller must relocate. */

    VgVec3 move = {input->move_world.x, input->move_world.y, 0.0f};
    float move_length = controller_length(move);
    if (move_length > 1.0f)
        move = controller_scale(move, 1.0f / move_length);
    move = controller_scale(move, config->move_speed * fixed_dt);
    bool was_grounded = state.grounded != 0u;
    if (was_grounded)
        state.last_ground_height = state.feet.z;
    VgVec3 feet_before_move = state.feet;
    float height_before_move = state.feet.z;
    state.hit_wall = 0u;
    state.hit_ceiling = 0u;
    for (uint32_t slide = 0u; slide < 3u && controller_length(move) > 1.0e-6f; ++slide) {
        bool hit = false;
        VgSpatialHit contact = {0};
        error = controller_sweep(scene, config, ignored_entity, state.feet, move, &hit, &contact);
        if (error != VG_OK)
            return error;
        if (!hit) {
            state.feet = controller_add(state.feet, move);
            break;
        }
        if (slide == 0u && was_grounded) {
            bool stepped = false;
            VgVec3 stepped_feet = {0};
            error = controller_try_step(scene, config, ignored_entity, state.feet, move,
                                        &stepped, &stepped_feet);
            if (error != VG_OK)
                return error;
            if (stepped) {
                state.feet = stepped_feet;
                move = (VgVec3){0};
                break;
            }
        }
        float length = controller_length(move);
        float advance = fmaxf(0.0f, contact.distance - VG_CONTROLLER_SKIN);
        if (advance > length)
            advance = length;
        VgVec3 direction = controller_scale(move, 1.0f / length);
        state.feet = controller_add(state.feet, controller_scale(direction, advance));
        move = controller_scale(direction, length - advance);
        float into = controller_dot(move, contact.normal);
        if (into < 0.0f)
            move = controller_add(move, controller_scale(contact.normal, -into));
        state.hit_wall = 1u;
    }
    float highest_allowed = fmaxf(height_before_move,
                                  state.last_ground_height + config->step_height);
    if (state.feet.z > highest_allowed)
        state.feet.z = highest_allowed;
    error = controller_overlap(scene, config, ignored_entity, state.feet, &overlap);
    if (error != VG_OK)
        return error;
    if (overlap) {
        state.feet = feet_before_move;
        state.hit_wall = 1u;
    }

    if (input->jump_pressed != 0u && was_grounded)
        state.vertical_speed = config->jump_speed;
    else if (was_grounded && state.vertical_speed < 0.0f)
        state.vertical_speed = 0.0f;
    state.vertical_speed -= config->gravity * fixed_dt;
    VgVec3 vertical = {0.0f, 0.0f, state.vertical_speed * fixed_dt};
    bool hit = false;
    VgSpatialHit contact = {0};
    error = controller_sweep(scene, config, ignored_entity, state.feet, vertical, &hit, &contact);
    if (error != VG_OK)
        return error;
    state.grounded = 0u;
    if (hit) {
        float advance = fmaxf(0.0f, contact.distance - VG_CONTROLLER_SKIN);
        if (advance > fabsf(vertical.z))
            advance = fabsf(vertical.z);
        state.feet.z += copysignf(advance, vertical.z);
        if (vertical.z < 0.0f && contact.normal.z >= config->min_ground_normal_z)
            state.grounded = 1u;
        if (vertical.z > 0.0f && contact.normal.z < 0.0f)
            state.hit_ceiling = 1u;
        state.vertical_speed = 0.0f;
    } else {
        state.feet.z += vertical.z;
    }
    if (state.vertical_speed <= 0.0f && input->jump_pressed == 0u) {
        bool ground = false;
        float drop = 0.0f;
        error = controller_ground(scene, config, ignored_entity, state.feet,
                                  config->ground_snap, &ground, &drop);
        if (error != VG_OK)
            return error;
        if (ground) {
            state.feet.z -= drop;
            state.vertical_speed = 0.0f;
            state.grounded = 1u;
        }
    }
    if (state.grounded != 0u)
        state.last_ground_height = state.feet.z;
    *in_out_state = state;
    return VG_OK;
}

VgVec3 vg_controller_eye_position(const VgControllerConfig *config,
                                   const VgControllerState *state) {
    if (config == NULL || state == NULL)
        return (VgVec3){0};
    VgVec3 eye = state->feet;
    eye.z += config->eye_height;
    return eye;
}

VgResult vg_controller_probe_segment(VgSpatialScene *scene, const VgControllerConfig *config,
                                      VgEntity ignored_entity, VgVec3 start_feet,
                                      VgVec3 end_feet, bool *out_reachable) {
    if (scene == NULL || !controller_valid_config(config) || !controller_finite_vec(start_feet) ||
        !controller_finite_vec(end_feet) || out_reachable == NULL)
        return VG_ERROR_INVALID_ARGUMENT;
    *out_reachable = false;
    VgVec3 delta = {end_feet.x - start_feet.x, end_feet.y - start_feet.y, 0.0f};
    float distance = controller_length(delta);
    if (distance > 20.0f)
        return VG_ERROR_UNSUPPORTED;
    VgVec3 direction = distance > 1.0e-6f ? controller_scale(delta, 1.0f / distance) :
                                                 (VgVec3){0};
    VgControllerState state = {0};
    state.feet = start_feet;
    state.grounded = 1u;
    float probe_dt = fminf(config->max_fixed_dt, 1.0f / 60.0f);
    if (distance > config->move_speed * probe_dt * 2046.0f)
        return VG_ERROR_UNSUPPORTED;
    for (uint32_t step = 0u; step < 2048u; ++step) {
        VgVec3 remain = {end_feet.x - state.feet.x, end_feet.y - state.feet.y, 0.0f};
        if (controller_length(remain) <= 0.08f) {
            *out_reachable = fabsf(end_feet.z - state.feet.z) <= 0.08f;
            return VG_OK;
        }
        VgControllerInput input = {direction, 0u};
        VgResult error = vg_controller_step(scene, config, ignored_entity, &input,
                                             probe_dt, &state);
        if (error != VG_OK)
            return error;
    }
    return VG_OK;
}
