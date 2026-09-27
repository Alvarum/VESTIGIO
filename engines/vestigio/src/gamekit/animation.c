#include "vestigio/animation.h"

#include <math.h>
#include <string.h>

static bool finite_vec(VgVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool finite_quat(VgQuat value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z) && isfinite(value.w);
}

VgResult vg_rigid_clip_validate(const VgRigidClip *clip) {
    if (clip == NULL || clip->keys == NULL || clip->key_count < 2u || clip->key_count > 1024u ||
        !isfinite(clip->duration_seconds) || clip->duration_seconds <= 0.0f ||
        clip->keys[0].seconds != 0.0f ||
        clip->keys[clip->key_count - 1u].seconds != clip->duration_seconds)
        return VG_ERROR_INVALID_ARGUMENT;
    float previous = -1.0f;
    for (size_t i = 0u; i < clip->key_count; ++i) {
        const VgRigidKeyframe *key = &clip->keys[i];
        float length_squared =
            key->rotation.x * key->rotation.x + key->rotation.y * key->rotation.y +
            key->rotation.z * key->rotation.z + key->rotation.w * key->rotation.w;
        if (!isfinite(key->seconds) || key->seconds <= previous || !finite_vec(key->translation) ||
            !finite_quat(key->rotation) || !finite_vec(key->scale) || key->scale.x <= 0.0f ||
            key->scale.y <= 0.0f || key->scale.z <= 0.0f || !isfinite(length_squared) ||
            length_squared < 0.000001f)
            return VG_ERROR_INVALID_ARGUMENT;
        previous = key->seconds;
    }
    return VG_OK;
}

static VgQuat normalized_lerp(VgQuat a, VgQuat b, float fraction) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f) {
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }
    VgQuat value = {a.x + (b.x - a.x) * fraction, a.y + (b.y - a.y) * fraction,
                    a.z + (b.z - a.z) * fraction, a.w + (b.w - a.w) * fraction};
    float reciprocal =
        1.0f / sqrtf(value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
    value.x *= reciprocal;
    value.y *= reciprocal;
    value.z *= reciprocal;
    value.w *= reciprocal;
    return value;
}

static VgQuat product(VgQuat a, VgQuat b) {
    return (VgQuat){a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                    a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                    a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                    a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

static float lerp(float a, float b, float fraction) {
    return a + (b - a) * fraction;
}

VgResult vg_rigid_instance_pose(const VgRigidInstance *instance, VgTransform *out_transform) {
    if (instance == NULL || out_transform == NULL || instance->clip == NULL ||
        vg_rigid_clip_validate(instance->clip) != VG_OK || !isfinite(instance->seconds))
        return VG_ERROR_INVALID_ARGUMENT;
    const VgRigidClip *clip = instance->clip;
    double time = clip->loop ? fmod(instance->seconds, clip->duration_seconds)
                             : fmin(instance->seconds, clip->duration_seconds);
    if (time < 0.0)
        time += clip->duration_seconds;
    size_t end = 1u;
    while (end + 1u < clip->key_count && (double)clip->keys[end].seconds < time)
        ++end;
    const VgRigidKeyframe *a = &clip->keys[end - 1u];
    const VgRigidKeyframe *b = &clip->keys[end];
    float fraction = (float)((time - a->seconds) / (b->seconds - a->seconds));
    VgTransform pose = instance->bind_transform;
    pose.position.x += lerp(a->translation.x, b->translation.x, fraction);
    pose.position.y += lerp(a->translation.y, b->translation.y, fraction);
    pose.position.z += lerp(a->translation.z, b->translation.z, fraction);
    pose.rotation = product(pose.rotation, normalized_lerp(a->rotation, b->rotation, fraction));
    pose.scale.x *= lerp(a->scale.x, b->scale.x, fraction);
    pose.scale.y *= lerp(a->scale.y, b->scale.y, fraction);
    pose.scale.z *= lerp(a->scale.z, b->scale.z, fraction);
    *out_transform = pose;
    return VG_OK;
}

VgResult vg_rigid_instance_bind(VgContext *context, VgEntity entity, const VgRigidClip *clip,
                                double start_seconds, VgRigidInstance *out_instance) {
    if (context == NULL || out_instance == NULL || !isfinite(start_seconds) ||
        start_seconds < 0.0 || vg_rigid_clip_validate(clip) != VG_OK)
        return VG_ERROR_INVALID_ARGUMENT;
    VgRigidInstance candidate = {0};
    VgResult result = vg_entity_get_local_transform(context, entity, &candidate.bind_transform);
    if (result != VG_OK)
        return result;
    candidate.entity = entity;
    candidate.clip = clip;
    candidate.seconds = start_seconds;
    candidate.speed = 1.0f;
    candidate.playing = true;
    VgTransform pose;
    result = vg_rigid_instance_pose(&candidate, &pose);
    if (result == VG_OK)
        result = vg_entity_set_local_transform(context, entity, &pose);
    if (result == VG_OK)
        *out_instance = candidate;
    return result;
}

VgResult vg_rigid_instance_step(VgContext *context, VgRigidInstance *instance,
                                double elapsed_seconds) {
    if (context == NULL || instance == NULL || instance->clip == NULL ||
        !isfinite(elapsed_seconds) || elapsed_seconds < 0.0 || !isfinite(instance->speed) ||
        instance->speed <= 0.0f)
        return VG_ERROR_INVALID_ARGUMENT;
    if (!instance->playing)
        return VG_OK;
    double next = instance->seconds + elapsed_seconds * instance->speed;
    if (!isfinite(next))
        return VG_ERROR_INVALID_ARGUMENT;
    double previous = instance->seconds;
    bool was_playing = instance->playing;
    if (instance->clip->loop)
        next = fmod(next, instance->clip->duration_seconds);
    else if (next >= instance->clip->duration_seconds) {
        next = instance->clip->duration_seconds;
        instance->playing = false;
    }
    instance->seconds = next;
    VgTransform pose;
    VgResult result = vg_rigid_instance_pose(instance, &pose);
    if (result == VG_OK)
        result = vg_entity_set_local_transform(context, instance->entity, &pose);
    if (result != VG_OK) {
        instance->seconds = previous;
        instance->playing = was_playing;
    }
    return result;
}
