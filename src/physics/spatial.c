#include "vestigio/spatial.h"

#include <float.h>
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define VG_SPATIAL_MAX_COORD 1000000.0f
#define VG_SPATIAL_MAX_DISTANCE 100000.0f
#define VG_SPATIAL_DEFAULT_MESHES 64u
#define VG_SPATIAL_DEFAULT_COLLIDERS 1024u
#define VG_SPATIAL_DEFAULT_TRIANGLES 8192u

/* Globally distinct live generations keep handles from different scenes from
 * accidentally aliasing when their first slot has the same ordinal. */
static atomic_uint_least32_t next_spatial_generation=1u;
static uint32_t new_generation(void) {
    uint32_t result=(uint32_t)atomic_fetch_add_explicit(&next_spatial_generation,1u,
                                                         memory_order_relaxed);
    return result ? result : (uint32_t)atomic_fetch_add_explicit(
        &next_spatial_generation,1u,memory_order_relaxed);
}

typedef struct VgBounds { VgVec3 min, max; } VgBounds;
typedef struct VgMeshSlot {
    VgVec3 *positions;
    uint32_t *indices;
    VgBounds *triangle_bounds;
    uint32_t position_count, index_count, generation, references;
    VgBounds bounds;
    bool alive;
} VgMeshSlot;
typedef struct VgColliderSlot {
    VgSpatialColliderDesc desc;
    VgBounds bounds;
    uint32_t generation;
    bool alive;
} VgColliderSlot;
struct VgSpatialScene {
    VgMeshSlot *meshes;
    VgColliderSlot *colliders;
    uint32_t max_meshes, max_colliders, max_triangles;
    uint32_t mesh_count, collider_count;
    uint64_t revision, broadphase_tests, triangle_tests;
    void *allocator_user;
    VgAllocateFn allocate;
    VgDeallocateFn deallocate;
};
typedef struct VgClosest {
    VgVec3 position, normal;
    float distance;
} VgClosest;

static VgVec3 v3(float x, float y, float z) { return (VgVec3){x, y, z}; }
static VgVec3 add(VgVec3 a, VgVec3 b) { return v3(a.x+b.x,a.y+b.y,a.z+b.z); }
static VgVec3 sub(VgVec3 a, VgVec3 b) { return v3(a.x-b.x,a.y-b.y,a.z-b.z); }
static VgVec3 mul(VgVec3 a, float b) { return v3(a.x*b,a.y*b,a.z*b); }
static float dot(VgVec3 a, VgVec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static VgVec3 cross(VgVec3 a, VgVec3 b) {
    return v3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);
}
static float length(VgVec3 a) { return sqrtf(dot(a,a)); }
static VgVec3 unit_or(VgVec3 a, VgVec3 fallback) {
    float n=length(a); return n>1.0e-12f ? mul(a,1.0f/n) : fallback;
}
static bool finite_vec(VgVec3 a) {
    return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z) &&
           fabsf(a.x)<=VG_SPATIAL_MAX_COORD && fabsf(a.y)<=VG_SPATIAL_MAX_COORD &&
           fabsf(a.z)<=VG_SPATIAL_MAX_COORD;
}
static float clamp(float x,float lo,float hi) { return fmaxf(lo,fminf(hi,x)); }
static VgVec3 rotate(VgQuat q,VgVec3 p) {
    VgVec3 u=v3(q.x,q.y,q.z), t=mul(cross(u,p),2.0f);
    return add(p,add(mul(t,q.w),cross(u,t)));
}
static VgVec3 transform_point(VgTransform t,VgVec3 p) {
    return add(t.position,rotate(t.rotation,v3(p.x*t.scale.x,p.y*t.scale.y,p.z*t.scale.z)));
}
static VgBounds empty_bounds(void) {
    return (VgBounds){v3(FLT_MAX,FLT_MAX,FLT_MAX),v3(-FLT_MAX,-FLT_MAX,-FLT_MAX)};
}
static void bounds_add(VgBounds *b,VgVec3 p) {
    b->min.x=fminf(b->min.x,p.x); b->min.y=fminf(b->min.y,p.y); b->min.z=fminf(b->min.z,p.z);
    b->max.x=fmaxf(b->max.x,p.x); b->max.y=fmaxf(b->max.y,p.y); b->max.z=fmaxf(b->max.z,p.z);
}
static VgBounds transform_bounds(VgTransform t,VgBounds b) {
    VgBounds r=empty_bounds();
    for(unsigned k=0u;k<8u;++k)
        bounds_add(&r,transform_point(t,v3((k&1u)?b.max.x:b.min.x,
                                           (k&2u)?b.max.y:b.min.y,
                                           (k&4u)?b.max.z:b.min.z)));
    return r;
}
static bool bounds_overlap(VgBounds a,VgBounds b) {
    return a.min.x<=b.max.x+VG_SPATIAL_EPSILON && a.max.x+VG_SPATIAL_EPSILON>=b.min.x &&
           a.min.y<=b.max.y+VG_SPATIAL_EPSILON && a.max.y+VG_SPATIAL_EPSILON>=b.min.y &&
           a.min.z<=b.max.z+VG_SPATIAL_EPSILON && a.max.z+VG_SPATIAL_EPSILON>=b.min.z;
}
static float bounds_distance(VgBounds b,VgVec3 p) {
    VgVec3 d=v3(fmaxf(fmaxf(b.min.x-p.x,0.0f),p.x-b.max.x),
                fmaxf(fmaxf(b.min.y-p.y,0.0f),p.y-b.max.y),
                fmaxf(fmaxf(b.min.z-p.z,0.0f),p.z-b.max.z));
    return length(d);
}
static VgBounds path_bounds(VgVec3 start,VgVec3 end,float radius) {
    VgBounds b=empty_bounds(); bounds_add(&b,start); bounds_add(&b,end);
    b.min=sub(b.min,v3(radius,radius,radius)); b.max=add(b.max,v3(radius,radius,radius)); return b;
}
static void *default_allocate(void *user,uint64_t size) {
    (void)user; return size<=SIZE_MAX ? malloc((size_t)size) : NULL;
}
static void default_deallocate(void *user,void *pointer) { (void)user; free(pointer); }
static void *alloc(VgSpatialScene *s,size_t count,size_t element) {
    if(count==0u || element==0u || count>SIZE_MAX/element) return NULL;
    void *p=s->allocate(s->allocator_user,(uint64_t)(count*element));
    if(p) memset(p,0,count*element);
    return p;
}
static void release(VgSpatialScene *s,void *pointer) {
    if(pointer) s->deallocate(s->allocator_user,pointer);
}
static uint64_t handle(uint32_t index,uint32_t generation) {
    return ((uint64_t)generation<<32u)|(uint64_t)(index+1u);
}
static VgMeshSlot *mesh_slot(VgSpatialScene *s,VgSpatialMesh mesh) {
    uint32_t ordinal=(uint32_t)mesh.value;
    if(!s || ordinal==0u || ordinal>s->max_meshes) return NULL;
    VgMeshSlot *slot=&s->meshes[ordinal-1u];
    return slot->alive && slot->generation==(uint32_t)(mesh.value>>32u) ? slot : NULL;
}
static VgColliderSlot *collider_slot(VgSpatialScene *s,VgSpatialCollider collider) {
    uint32_t ordinal=(uint32_t)collider.value;
    if(!s || ordinal==0u || ordinal>s->max_colliders) return NULL;
    VgColliderSlot *slot=&s->colliders[ordinal-1u];
    return slot->alive && slot->generation==(uint32_t)(collider.value>>32u) ? slot : NULL;
}

VgResult vg_spatial_scene_create(const VgSpatialSceneConfig *config,VgSpatialScene **out_scene) {
    if(!out_scene || (config && ((config->allocate==NULL)!=(config->deallocate==NULL))))
        return VG_ERROR_INVALID_ARGUMENT;
    *out_scene=NULL;
    VgAllocateFn a=config&&config->allocate?config->allocate:default_allocate;
    VgDeallocateFn d=config&&config->deallocate?config->deallocate:default_deallocate;
    void *user=config?config->allocator_user:NULL;
    uint32_t meshes=config&&config->max_meshes?config->max_meshes:VG_SPATIAL_DEFAULT_MESHES;
    uint32_t colliders=config&&config->max_colliders?config->max_colliders:VG_SPATIAL_DEFAULT_COLLIDERS;
    uint32_t triangles=config&&config->max_triangles_per_mesh?config->max_triangles_per_mesh:VG_SPATIAL_DEFAULT_TRIANGLES;
    if(meshes>UINT16_MAX || colliders>UINT16_MAX || triangles>UINT16_MAX)
        return VG_ERROR_CAPACITY;
    VgSpatialScene *s=a(user,sizeof(*s)); if(!s) return VG_ERROR_OUT_OF_MEMORY;
    memset(s,0,sizeof(*s)); s->allocate=a; s->deallocate=d; s->allocator_user=user;
    s->max_meshes=meshes; s->max_colliders=colliders; s->max_triangles=triangles;
    s->meshes=alloc(s,meshes,sizeof(*s->meshes));
    s->colliders=alloc(s,colliders,sizeof(*s->colliders));
    if(!s->meshes || !s->colliders) { vg_spatial_scene_destroy(s); return VG_ERROR_OUT_OF_MEMORY; }
    *out_scene=s; return VG_OK;
}
void vg_spatial_scene_destroy(VgSpatialScene *s) {
    if(!s) return;
    if(s->meshes) for(uint32_t i=0u;i<s->max_meshes;++i) {
        release(s,s->meshes[i].positions);
        release(s,s->meshes[i].indices);
        release(s,s->meshes[i].triangle_bounds);
    }
    release(s,s->meshes); release(s,s->colliders);
    s->deallocate(s->allocator_user,s);
}

static VgResult mesh_data_validate(VgSpatialScene *s,const VgSpatialMeshData *data) {
    if(!s || !data || !data->positions || !data->indices || data->position_count<3u ||
       data->index_count==0u || data->index_count%3u!=0u) return VG_ERROR_INVALID_ARGUMENT;
    if(data->index_count/3u>s->max_triangles) return VG_ERROR_CAPACITY;
    if(data->position_count>s->max_triangles*3u) return VG_ERROR_CAPACITY;
    for(uint32_t i=0u;i<data->position_count;++i)
        if(!finite_vec(data->positions[i])) return VG_ERROR_INVALID_ARGUMENT;
    for(uint32_t i=0u;i<data->index_count;++i)
        if(data->indices[i]>=data->position_count) return VG_ERROR_INVALID_ARGUMENT;
    for(uint32_t i=0u;i<data->index_count;i+=3u) {
        VgVec3 a=data->positions[data->indices[i]],b=data->positions[data->indices[i+1u]];
        VgVec3 c=data->positions[data->indices[i+2u]];
        if(length(cross(sub(b,a),sub(c,a)))<=VG_SPATIAL_EPSILON)
            return VG_ERROR_INVALID_ARGUMENT;
    }
    return VG_OK;
}
static VgResult mesh_prepare(VgSpatialScene *s,const VgSpatialMeshData *data,VgMeshSlot *candidate) {
    memset(candidate,0,sizeof(*candidate));
    candidate->positions=alloc(s,data->position_count,sizeof(VgVec3));
    candidate->indices=alloc(s,data->index_count,sizeof(uint32_t));
    candidate->triangle_bounds=alloc(s,data->index_count/3u,sizeof(VgBounds));
    if(!candidate->positions || !candidate->indices || !candidate->triangle_bounds) {
        release(s,candidate->positions);
        release(s,candidate->indices);
        release(s,candidate->triangle_bounds);
        return VG_ERROR_OUT_OF_MEMORY;
    }
    memcpy(candidate->positions,data->positions,(size_t)data->position_count*sizeof(VgVec3));
    memcpy(candidate->indices,data->indices,(size_t)data->index_count*sizeof(uint32_t));
    candidate->position_count=data->position_count; candidate->index_count=data->index_count;
    candidate->bounds=empty_bounds();
    for(uint32_t i=0u;i<data->index_count/3u;++i) {
        VgBounds b=empty_bounds();
        for(uint32_t k=0u;k<3u;++k) bounds_add(&b,data->positions[data->indices[i*3u+k]]);
        candidate->triangle_bounds[i]=b;
        bounds_add(&candidate->bounds,b.min); bounds_add(&candidate->bounds,b.max);
    }
    return VG_OK;
}
VgResult vg_spatial_mesh_create(VgSpatialScene *s,const VgSpatialMeshData *data,VgSpatialMesh *out) {
    if(!out) return VG_ERROR_INVALID_ARGUMENT;
    out->value=0u; VgResult r=mesh_data_validate(s,data); if(r!=VG_OK) return r;
    for(uint32_t i=0u;i<s->max_meshes;++i) if(!s->meshes[i].alive) {
        VgMeshSlot candidate; r=mesh_prepare(s,data,&candidate); if(r!=VG_OK) return r;
        candidate.generation=new_generation();
        candidate.alive=true; s->meshes[i]=candidate;
        s->mesh_count++; s->revision++; out->value=handle(i,candidate.generation); return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}
VgResult vg_spatial_mesh_replace(VgSpatialScene *s,VgSpatialMesh mesh,const VgSpatialMeshData *data) {
    VgMeshSlot *old=mesh_slot(s,mesh); if(!old) return VG_ERROR_INVALID_HANDLE;
    VgResult r=mesh_data_validate(s,data); if(r!=VG_OK) return r;
    VgMeshSlot replacement; r=mesh_prepare(s,data,&replacement); if(r!=VG_OK) return r;
    replacement.generation=old->generation; replacement.references=old->references;
    replacement.alive=true;
    release(s,old->positions);
    release(s,old->indices);
    release(s,old->triangle_bounds);
    *old=replacement;
    for(uint32_t i=0u;i<s->max_colliders;++i) if(s->colliders[i].alive &&
            s->colliders[i].desc.shape_type==VG_SPATIAL_SHAPE_STATIC_MESH &&
            s->colliders[i].desc.shape.mesh.value==mesh.value)
        s->colliders[i].bounds=transform_bounds(s->colliders[i].desc.transform,old->bounds);
    s->revision++; return VG_OK;
}
VgResult vg_spatial_mesh_destroy(VgSpatialScene *s,VgSpatialMesh mesh) {
    VgMeshSlot *slot=mesh_slot(s,mesh); if(!slot) return VG_ERROR_INVALID_HANDLE;
    if(slot->references) return VG_ERROR_CONFLICT;
    release(s,slot->positions);
    release(s,slot->indices);
    release(s,slot->triangle_bounds);
    slot->positions=NULL; slot->indices=NULL; slot->triangle_bounds=NULL;
    slot->alive=false; s->mesh_count--; s->revision++; return VG_OK;
}

static bool valid_transform(VgTransform t) {
    if(!finite_vec(t.position) || !finite_vec(t.scale) || t.scale.x<=VG_SPATIAL_EPSILON ||
       t.scale.y<=VG_SPATIAL_EPSILON || t.scale.z<=VG_SPATIAL_EPSILON ||
       t.scale.x>10000.0f || t.scale.y>10000.0f || t.scale.z>10000.0f ||
       !isfinite(t.rotation.x) || !isfinite(t.rotation.y) ||
       !isfinite(t.rotation.z) || !isfinite(t.rotation.w)) return false;
    float q=t.rotation.x*t.rotation.x+t.rotation.y*t.rotation.y+
            t.rotation.z*t.rotation.z+t.rotation.w*t.rotation.w;
    return fabsf(q-1.0f)<1.0e-3f;
}
static bool uniform_scale(VgTransform t) {
    return fabsf(t.scale.x-t.scale.y)<=VG_SPATIAL_EPSILON &&
           fabsf(t.scale.x-t.scale.z)<=VG_SPATIAL_EPSILON;
}
static VgResult desc_validate(VgSpatialScene *s,const VgSpatialColliderDesc *d) {
    if(!s || !d || !valid_transform(d->transform) || !d->layer_mask)
        return VG_ERROR_INVALID_ARGUMENT;
    switch(d->shape_type) {
    case VG_SPATIAL_SHAPE_BOX:
        if(!finite_vec(d->shape.box.center) || !finite_vec(d->shape.box.half_extents) ||
           d->shape.box.half_extents.x<=0.0f || d->shape.box.half_extents.y<=0.0f ||
           d->shape.box.half_extents.z<=0.0f) return VG_ERROR_INVALID_ARGUMENT;
        break;
    case VG_SPATIAL_SHAPE_SPHERE:
        if(!uniform_scale(d->transform) || !finite_vec(d->shape.sphere.center) ||
           !isfinite(d->shape.sphere.radius) || d->shape.sphere.radius<=0.0f ||
           d->shape.sphere.radius>VG_SPATIAL_MAX_DISTANCE) return VG_ERROR_INVALID_ARGUMENT;
        break;
    case VG_SPATIAL_SHAPE_CAPSULE:
        if(!uniform_scale(d->transform) || !finite_vec(d->shape.capsule.point_a) ||
           !finite_vec(d->shape.capsule.point_b) || !isfinite(d->shape.capsule.radius) ||
           d->shape.capsule.radius<=0.0f || d->shape.capsule.radius>VG_SPATIAL_MAX_DISTANCE)
            return VG_ERROR_INVALID_ARGUMENT;
        break;
    case VG_SPATIAL_SHAPE_STATIC_MESH:
        if(!uniform_scale(d->transform) || !mesh_slot(s,d->shape.mesh))
            return VG_ERROR_INVALID_ARGUMENT;
        break;
    default: return VG_ERROR_INVALID_ARGUMENT;
    }
    return VG_OK;
}
static VgBounds collider_bounds(VgSpatialScene *s,const VgSpatialColliderDesc *d) {
    VgBounds b=empty_bounds();
    switch(d->shape_type) {
    case VG_SPATIAL_SHAPE_BOX: {
        VgVec3 c=d->shape.box.center,h=d->shape.box.half_extents;
        b.min=sub(c,h); b.max=add(c,h); return transform_bounds(d->transform,b);
    }
    case VG_SPATIAL_SHAPE_SPHERE: {
        VgVec3 c=transform_point(d->transform,d->shape.sphere.center);
        float r=d->shape.sphere.radius*d->transform.scale.x;
        b.min=sub(c,v3(r,r,r)); b.max=add(c,v3(r,r,r)); return b;
    }
    case VG_SPATIAL_SHAPE_CAPSULE: {
        bounds_add(&b,transform_point(d->transform,d->shape.capsule.point_a));
        bounds_add(&b,transform_point(d->transform,d->shape.capsule.point_b));
        float r=d->shape.capsule.radius*d->transform.scale.x;
        b.min=sub(b.min,v3(r,r,r)); b.max=add(b.max,v3(r,r,r)); return b;
    }
    default: return transform_bounds(d->transform,mesh_slot(s,d->shape.mesh)->bounds);
    }
}
VgResult vg_spatial_collider_create(VgSpatialScene *s,const VgSpatialColliderDesc *d,
                                    VgSpatialCollider *out) {
    if(!out) return VG_ERROR_INVALID_ARGUMENT;
    out->value=0u; VgResult r=desc_validate(s,d); if(r!=VG_OK) return r;
    for(uint32_t i=0u;i<s->max_colliders;++i) if(!s->colliders[i].alive) {
        VgColliderSlot *slot=&s->colliders[i];
        slot->generation=new_generation();
        slot->desc=*d; slot->bounds=collider_bounds(s,d); slot->alive=true;
        if(d->shape_type==VG_SPATIAL_SHAPE_STATIC_MESH)
            mesh_slot(s,d->shape.mesh)->references++;
        s->collider_count++; s->revision++;
        out->value=handle(i,slot->generation); return VG_OK;
    }
    return VG_ERROR_CAPACITY;
}
VgResult vg_spatial_collider_update(VgSpatialScene *s,VgSpatialCollider collider,
                                    const VgSpatialColliderDesc *d) {
    VgColliderSlot *slot=collider_slot(s,collider); if(!slot) return VG_ERROR_INVALID_HANDLE;
    VgResult r=desc_validate(s,d); if(r!=VG_OK) return r;
    VgBounds bounds=collider_bounds(s,d);
    if(slot->desc.shape_type==VG_SPATIAL_SHAPE_STATIC_MESH)
        mesh_slot(s,slot->desc.shape.mesh)->references--;
    if(d->shape_type==VG_SPATIAL_SHAPE_STATIC_MESH)
        mesh_slot(s,d->shape.mesh)->references++;
    slot->desc=*d; slot->bounds=bounds; s->revision++; return VG_OK;
}
VgResult vg_spatial_collider_destroy(VgSpatialScene *s,VgSpatialCollider collider) {
    VgColliderSlot *slot=collider_slot(s,collider); if(!slot) return VG_ERROR_INVALID_HANDLE;
    if(slot->desc.shape_type==VG_SPATIAL_SHAPE_STATIC_MESH)
        mesh_slot(s,slot->desc.shape.mesh)->references--;
    slot->alive=false; s->collider_count--; s->revision++; return VG_OK;
}

/* Closest point on a triangle (Ericson's Voronoi-region method). */
static VgVec3 closest_triangle(VgVec3 p,VgVec3 a,VgVec3 b,VgVec3 c) {
    VgVec3 ab=sub(b,a),ac=sub(c,a),ap=sub(p,a);
    float d1=dot(ab,ap),d2=dot(ac,ap);
    if(d1<=0.0f && d2<=0.0f) return a;
    VgVec3 bp=sub(p,b); float d3=dot(ab,bp),d4=dot(ac,bp);
    if(d3>=0.0f && d4<=d3) return b;
    float vc=d1*d4-d3*d2;
    if(vc<=0.0f && d1>=0.0f && d3<=0.0f) return add(a,mul(ab,d1/(d1-d3)));
    VgVec3 cp=sub(p,c); float d5=dot(ab,cp),d6=dot(ac,cp);
    if(d6>=0.0f && d5<=d6) return c;
    float vb=d5*d2-d1*d6;
    if(vb<=0.0f && d2>=0.0f && d6<=0.0f) return add(a,mul(ac,d2/(d2-d6)));
    float va=d3*d6-d5*d4;
    if(va<=0.0f && (d4-d3)>=0.0f && (d5-d6)>=0.0f)
        return add(b,mul(sub(c,b),(d4-d3)/((d4-d3)+(d5-d6))));
    float inv=1.0f/(va+vb+vc);
    return add(a,add(mul(ab,vb*inv),mul(ac,vc*inv)));
}
static VgClosest closest_triangle_points(VgSpatialScene *s,VgVec3 p,
                                         VgVec3 a,VgVec3 b,VgVec3 c) {
    VgVec3 nearest=closest_triangle(p,a,b,c),offset=sub(p,nearest);
    float distance=length(offset); s->triangle_tests++;
    VgVec3 normal=unit_or(offset,unit_or(cross(sub(b,a),sub(c,a)),v3(0,0,1)));
    return (VgClosest){nearest,normal,distance};
}
static VgClosest closest_box(const VgSpatialBox *box,VgTransform t,VgVec3 p) {
    VgVec3 c=transform_point(t,box->center);
    VgVec3 axes[3]={rotate(t.rotation,v3(1,0,0)),rotate(t.rotation,v3(0,1,0)),
                    rotate(t.rotation,v3(0,0,1))};
    float h[3]={box->half_extents.x*t.scale.x,box->half_extents.y*t.scale.y,
                box->half_extents.z*t.scale.z};
    VgVec3 delta=sub(p,c),nearest=c; float q[3];
    bool outside=false;
    for(unsigned k=0u;k<3u;++k) {
        q[k]=dot(delta,axes[k]);
        if(fabsf(q[k])>h[k]) outside=true;
        nearest=add(nearest,mul(axes[k],clamp(q[k],-h[k],h[k])));
    }
    VgClosest result={nearest,v3(0,0,1),0};
    if(outside) {
        VgVec3 offset=sub(p,nearest);
        result.distance=length(offset);
        result.normal=unit_or(offset,result.normal);
    } else {
        unsigned best=0u; float margin=h[0]-fabsf(q[0]);
        for(unsigned k=1u;k<3u;++k) if(h[k]-fabsf(q[k])<margin) {
            best=k; margin=h[k]-fabsf(q[k]);
        }
        float sign=q[best]>=0.0f?1.0f:-1.0f;
        result.normal=mul(axes[best],sign);
        result.position=add(nearest,mul(axes[best],sign*h[best]-q[best]));
        result.distance=-margin;
    }
    return result;
}
static VgClosest closest_collider(VgSpatialScene *s,VgColliderSlot *slot,VgVec3 p) {
    const VgSpatialColliderDesc *d=&slot->desc;
    if(d->shape_type==VG_SPATIAL_SHAPE_BOX)
        return closest_box(&d->shape.box,d->transform,p);
    if(d->shape_type==VG_SPATIAL_SHAPE_SPHERE || d->shape_type==VG_SPATIAL_SHAPE_CAPSULE) {
        VgVec3 center;
        float radius;
        if(d->shape_type==VG_SPATIAL_SHAPE_SPHERE) {
            center=transform_point(d->transform,d->shape.sphere.center);
            radius=d->shape.sphere.radius*d->transform.scale.x;
        } else {
            VgVec3 a=transform_point(d->transform,d->shape.capsule.point_a);
            VgVec3 b=transform_point(d->transform,d->shape.capsule.point_b),ab=sub(b,a);
            float k=dot(ab,ab)>0.0f?clamp(dot(sub(p,a),ab)/dot(ab,ab),0.0f,1.0f):0.0f;
            center=add(a,mul(ab,k)); radius=d->shape.capsule.radius*d->transform.scale.x;
        }
        VgVec3 delta=sub(p,center); float dist=length(delta);
        VgVec3 normal=unit_or(delta,v3(0,0,1));
        return (VgClosest){add(center,mul(normal,radius)),normal,dist-radius};
    }
    VgMeshSlot *mesh=mesh_slot(s,d->shape.mesh);
    VgClosest result={v3(0,0,0),v3(0,0,1),FLT_MAX};
    for(uint32_t i=0u;i<mesh->index_count/3u;++i) {
        VgBounds triangle_bounds=transform_bounds(d->transform,mesh->triangle_bounds[i]);
        if(bounds_distance(triangle_bounds,p)>result.distance) continue;
        VgVec3 a=transform_point(d->transform,mesh->positions[mesh->indices[i*3u]]);
        VgVec3 b=transform_point(d->transform,mesh->positions[mesh->indices[i*3u+1u]]);
        VgVec3 c=transform_point(d->transform,mesh->positions[mesh->indices[i*3u+2u]]);
        VgClosest candidate=closest_triangle_points(s,p,a,b,c);
        if(candidate.distance<result.distance) result=candidate;
    }
    return result;
}
static VgClosest closest_feature(VgSpatialScene *s,VgColliderSlot *slot,VgMeshSlot *mesh,
                                 VgVec3 a,VgVec3 b,VgVec3 c,VgVec3 p) {
    return mesh ? closest_triangle_points(s,p,a,b,c) : closest_collider(s,slot,p);
}
static float feature_gap(VgSpatialScene *s,VgColliderSlot *slot,VgMeshSlot *mesh,
                         VgVec3 a,VgVec3 b,VgVec3 c,VgVec3 center,VgVec3 travel,
                         float t,float radius) {
    return closest_feature(s,slot,mesh,a,b,c,add(center,mul(travel,t))).distance-radius;
}

static bool eligible(const VgColliderSlot *slot,uint64_t mask,VgEntity ignored) {
    return slot->alive && slot->desc.enabled && (slot->desc.layer_mask&mask)!=0u &&
           (ignored.value==0u || slot->desc.entity.value!=ignored.value);
}
static VgSpatialHit make_hit(uint32_t index,const VgColliderSlot *slot,VgClosest closest,
                             float distance,float fraction) {
    VgSpatialHit h={0};
    h.entity=slot->desc.entity; h.collider.value=handle(index,slot->generation);
    h.position=closest.position; h.normal=closest.normal;
    h.distance=distance; h.fraction=fraction;
    return h;
}
VgResult vg_spatial_overlap_sphere(VgSpatialScene *s,const VgSpatialSphereQuery *q,
                                   bool *out_hit,VgSpatialHit *out_result) {
    if(!s || !q || !out_hit || !out_result || !finite_vec(q->center) ||
       !isfinite(q->radius) || q->radius<0.0f || q->radius>VG_SPATIAL_MAX_DISTANCE ||
       !q->layer_mask) return VG_ERROR_INVALID_ARGUMENT;
    *out_hit=false; memset(out_result,0,sizeof(*out_result));
    VgBounds query=path_bounds(q->center,q->center,q->radius);
    float best=FLT_MAX;
    for(uint32_t i=0u;i<s->max_colliders;++i) {
        VgColliderSlot *slot=&s->colliders[i];
        if(!eligible(slot,q->layer_mask,q->ignored_entity)) continue;
        s->broadphase_tests++;
        if(!bounds_overlap(query,slot->bounds)) continue;
        VgClosest c=closest_collider(s,slot,q->center);
        float separation=c.distance-q->radius;
        if(separation<=VG_SPATIAL_EPSILON && separation<best) {
            best=separation; *out_result=make_hit(i,slot,c,fminf(separation,0.0f),0.0f);
            *out_hit=true;
        }
    }
    return VG_OK;
}
static VgResult sweep(VgSpatialScene *s,VgVec3 center,float radius,VgVec3 direction,
                      float max_distance,uint64_t mask,VgEntity ignored,
                      bool *out_hit,VgSpatialHit *out_result) {
    if(!s || !out_hit || !out_result || !finite_vec(center) || !finite_vec(direction) ||
       !isfinite(radius) || radius<0.0f || radius>VG_SPATIAL_MAX_DISTANCE ||
       !isfinite(max_distance) || max_distance<0.0f ||
       max_distance>VG_SPATIAL_MAX_DISTANCE || !mask)
        return VG_ERROR_INVALID_ARGUMENT;
    float direction_length=length(direction);
    if(max_distance>0.0f && direction_length<=VG_SPATIAL_EPSILON)
        return VG_ERROR_INVALID_ARGUMENT;
    VgVec3 travel=direction_length>VG_SPATIAL_EPSILON?mul(direction,1.0f/direction_length):v3(0,0,0);
    VgBounds broad=path_bounds(center,add(center,mul(travel,max_distance)),radius);
    *out_hit=false; memset(out_result,0,sizeof(*out_result));
    float best=max_distance+VG_SPATIAL_EPSILON;
    for(uint32_t i=0u;i<s->max_colliders;++i) {
        VgColliderSlot *slot=&s->colliders[i];
        if(!eligible(slot,mask,ignored)) continue;
        s->broadphase_tests++;
        if(!bounds_overlap(broad,slot->bounds)) continue;
        /* Static meshes are swept triangle by triangle. Advancing against
         * the nearest triangle of the whole mesh can stall beside a long
         * noncontact floor and miss a later wall in that same mesh. */
        VgMeshSlot *mesh=slot->desc.shape_type==VG_SPATIAL_SHAPE_STATIC_MESH ?
                         mesh_slot(s,slot->desc.shape.mesh):NULL;
        uint32_t feature_count=mesh ? mesh->index_count/3u : 1u;
        for(uint32_t feature=0u;feature<feature_count;++feature) {
            VgVec3 a=v3(0,0,0),b=v3(0,0,0),c=v3(0,0,0);
            if(mesh) {
                VgBounds feature_bounds=transform_bounds(slot->desc.transform,
                                                          mesh->triangle_bounds[feature]);
                if(!bounds_overlap(broad,feature_bounds)) continue;
                a=transform_point(slot->desc.transform,
                                  mesh->positions[mesh->indices[feature*3u]]);
                b=transform_point(slot->desc.transform,
                                  mesh->positions[mesh->indices[feature*3u+1u]]);
                c=transform_point(slot->desc.transform,
                                  mesh->positions[mesh->indices[feature*3u+2u]]);
            }
            float t=0.0f;
            unsigned step=0u;
            for(;step<128u && t<=max_distance+VG_SPATIAL_EPSILON;++step) {
                VgVec3 point=add(center,mul(travel,t));
                VgClosest nearest=closest_feature(s,slot,mesh,a,b,c,point);
                float gap=nearest.distance-radius;
                if(gap<=VG_SPATIAL_EPSILON) {
                    /* Initial penetration must be returned for depenetration.
                     * A tangent with lateral/outward travel does not block. */
                    if(t<=VG_SPATIAL_EPSILON && gap>=0.0f &&
                       dot(travel,nearest.normal)>=0.0f &&
                       max_distance>0.0f) break;
                    if(t<best || (!*out_hit && t<=best)) {
                        best=t; *out_hit=true;
                        *out_result=make_hit(i,slot,nearest,
                            t<=VG_SPATIAL_EPSILON && gap<0.0f?gap:t,
                            max_distance>0.0f?t/max_distance:0.0f);
                    }
                    break;
                }
                /* Outside a convex primitive/triangle, distance along a
                 * straight path is convex. Once its directional derivative
                 * is nonnegative, this feature cannot be hit later. This
                 * avoids tiny fixed advances beside a near-tangent floor. */
                if(dot(travel,nearest.normal)>=0.0f)
                    break;
                if(t>=best || t>=max_distance) break;
                float advance=fmaxf(gap-VG_SPATIAL_EPSILON,VG_SPATIAL_EPSILON*0.25f);
                t+=advance;
                if(t>max_distance && t-max_distance<=VG_SPATIAL_EPSILON)
                    t=max_distance;
            }
            if(step==128u && t<=max_distance+VG_SPATIAL_EPSILON && t<best) {
                /* Conservative advancement can make arbitrarily small steps
                 * near a grazing contact. Distance to each convex feature is
                 * convex along the ray: locate its minimum, then bisect the
                 * first crossing. This avoids a silent miss or spurious
                 * iteration-budget error on long near-parallel travel. */
                float low=t,high=fminf(max_distance,best);
                for(unsigned search=0u;search<48u;++search) {
                    float third=(high-low)/3.0f;
                    if(third<=0.0f) break;
                    float m1=low+third,m2=high-third;
                    float g1=feature_gap(s,slot,mesh,a,b,c,center,travel,m1,radius);
                    float g2=feature_gap(s,slot,mesh,a,b,c,center,travel,m2,radius);
                    if(g1<=g2) high=m2; else low=m1;
                }
                float minimum=(low+high)*0.5f;
                if(feature_gap(s,slot,mesh,a,b,c,center,travel,minimum,radius)<=
                   VG_SPATIAL_EPSILON) {
                    low=t; high=minimum;
                    for(unsigned search=0u;search<40u && high>low;++search) {
                        float middle=(low+high)*0.5f;
                        if(middle<=low || middle>=high) break;
                        float gap=feature_gap(s,slot,mesh,a,b,c,center,travel,middle,radius);
                        if(gap<=VG_SPATIAL_EPSILON) high=middle; else low=middle;
                    }
                    if(high<best) {
                        VgClosest nearest=closest_feature(s,slot,mesh,a,b,c,
                                                           add(center,mul(travel,high)));
                        best=high; *out_hit=true;
                        *out_result=make_hit(i,slot,nearest,high,
                                             max_distance>0.0f?high/max_distance:0.0f);
                    }
                }
            }
        }
    }
    return VG_OK;
}
VgResult vg_spatial_sweep_sphere(VgSpatialScene *s,const VgSpatialSphereSweep *q,
                                 bool *out_hit,VgSpatialHit *out_result) {
    if(!q) return VG_ERROR_INVALID_ARGUMENT;
    return sweep(s,q->center,q->radius,q->direction,q->max_distance,q->layer_mask,
                 q->ignored_entity,out_hit,out_result);
}
VgResult vg_spatial_raycast(VgSpatialScene *s,const VgSpatialRayQuery *q,
                            bool *out_hit,VgSpatialHit *out_result) {
    if(!q || q->max_distance<=0.0f) return VG_ERROR_INVALID_ARGUMENT;
    return sweep(s,q->origin,0.0f,q->direction,q->max_distance,q->layer_mask,
                 q->ignored_entity,out_hit,out_result);
}
VgSpatialStats vg_spatial_scene_stats(const VgSpatialScene *s) {
    if(!s) return (VgSpatialStats){0};
    return (VgSpatialStats){s->mesh_count,s->collider_count,s->revision,
                            s->broadphase_tests,s->triangle_tests};
}
VgResult vg_spatial_scene_visit_debug(const VgSpatialScene *s,VgSpatialDebugVisitFn visit,
                                      void *user) {
    if(!s || !visit) return VG_ERROR_INVALID_ARGUMENT;
    for(uint32_t i=0u;i<s->max_colliders;++i) if(s->colliders[i].alive)
        visit(user,(VgSpatialCollider){handle(i,s->colliders[i].generation)},
              &s->colliders[i].desc,s->colliders[i].bounds.min,s->colliders[i].bounds.max);
    return VG_OK;
}
VgResult vg_spatial_mesh_visit_triangles(const VgSpatialScene *s,VgSpatialMesh mesh,
                                         VgSpatialTriangleVisitFn visit,void *user) {
    if(!s || !visit) return VG_ERROR_INVALID_ARGUMENT;
    VgMeshSlot *slot=mesh_slot((VgSpatialScene *)s,mesh);
    if(!slot) return VG_ERROR_INVALID_HANDLE;
    for(uint32_t i=0u;i<slot->index_count/3u;++i)
        visit(user,i,slot->positions[slot->indices[i*3u]],
              slot->positions[slot->indices[i*3u+1u]],
              slot->positions[slot->indices[i*3u+2u]]);
    return VG_OK;
}
