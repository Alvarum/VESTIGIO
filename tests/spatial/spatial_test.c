#include "vestigio/spatial.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(test) do { if(!(test)) { \
    fprintf(stderr,"spatial failure at line %d: %s\n",__LINE__,#test); exit(1); \
} } while(0)

static VgTransform identity(void) {
    VgTransform t={0}; t.rotation.w=1.0f; t.scale=(VgVec3){1,1,1}; return t;
}
static VgSpatialColliderDesc box_desc(VgEntity entity,VgVec3 center,VgVec3 half,uint64_t mask) {
    VgSpatialColliderDesc d={0}; d.entity=entity; d.layer_mask=mask;
    d.transform=identity(); d.shape_type=VG_SPATIAL_SHAPE_BOX; d.enabled=true;
    d.shape.box.center=center; d.shape.box.half_extents=half; return d;
}
typedef struct MeshVisit {
    uint32_t count;
    float first_x;
} MeshVisit;
static void visit_mesh(void *user,uint32_t triangle,VgVec3 a,VgVec3 b,VgVec3 c) {
    MeshVisit *visit=user;
    CHECK(triangle==visit->count);
    CHECK(fabsf(a.x-b.x)<1e-5f && fabsf(a.x-c.x)<1e-5f);
    visit->first_x=a.x; visit->count++;
}
static void test_box_and_filters(VgSpatialScene *s) {
    VgSpatialColliderDesc d=box_desc((VgEntity){11u},(VgVec3){0,0,0},(VgVec3){1,1,1},1u);
    VgSpatialCollider box={0}; CHECK(vg_spatial_collider_create(s,&d,&box)==VG_OK);
    VgSpatialRayQuery ray={{-3,0,0},{1,0,0},10,1u,{0}};
    VgSpatialHit hit={0}; bool found=false;
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK);
    CHECK(found && hit.collider.value==box.value && fabsf(hit.distance-2.0f)<1e-3f);
    CHECK(fabsf(hit.fraction-0.2f)<1e-3f && hit.normal.x < -0.99f);
    ray.layer_mask=2u; CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && !found);
    ray.layer_mask=1u; ray.ignored_entity=d.entity;
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && !found);
    ray.ignored_entity.value=0u;

    VgSpatialSphereQuery overlap={{2,0,0},1,1u,{0}};
    CHECK(vg_spatial_overlap_sphere(s,&overlap,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance)<1e-4f && hit.normal.x>0.99f);
    overlap.center.x=1.8f;
    CHECK(vg_spatial_overlap_sphere(s,&overlap,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance+0.2f)<1e-3f);
    overlap.center=(VgVec3){0,0,0}; overlap.radius=0.5f;
    CHECK(vg_spatial_overlap_sphere(s,&overlap,&found,&hit)==VG_OK && found);
    CHECK(hit.distance < -1.0f && hit.normal.x>0.99f);

    VgSpatialSphereSweep sweep={{-3,0,0},0.5f,{1,0,0},10,1u,{0}};
    CHECK(vg_spatial_sweep_sphere(s,&sweep,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance-1.5f)<1e-3f && hit.normal.x < -0.99f);
    sweep.center=(VgVec3){0,0,0};
    CHECK(vg_spatial_sweep_sphere(s,&sweep,&found,&hit)==VG_OK && found);
    CHECK(hit.distance < -1.0f && hit.fraction==0.0f);
    sweep.center=(VgVec3){-1.5f,0,0}; sweep.direction=(VgVec3){0,1,0};
    CHECK(vg_spatial_sweep_sphere(s,&sweep,&found,&hit)==VG_OK && !found);
    sweep.center=(VgVec3){-3,-3,0}; sweep.direction=(VgVec3){1,1,0};
    CHECK(vg_spatial_sweep_sphere(s,&sweep,&found,&hit)==VG_OK && found);

    d.transform.scale=(VgVec3){2,1,1};
    CHECK(vg_spatial_collider_update(s,box,&d)==VG_OK);
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance-1.0f)<1e-3f);
    CHECK(vg_spatial_collider_destroy(s,box)==VG_OK);
    CHECK(vg_spatial_collider_destroy(s,box)==VG_ERROR_INVALID_HANDLE);
}
static void test_mesh_and_revision(VgSpatialScene *s) {
    const VgVec3 positions[]={{0,-1,-1},{0,1,-1},{0,0,1}};
    const uint32_t indices[]={0,1,2};
    VgSpatialMeshData data={positions,3,indices,3};
    VgSpatialMesh mesh={0}; CHECK(vg_spatial_mesh_create(s,&data,&mesh)==VG_OK);
    MeshVisit visited={0};
    CHECK(vg_spatial_mesh_visit_triangles(s,mesh,visit_mesh,&visited)==VG_OK);
    CHECK(visited.count==1u && fabsf(visited.first_x)<1e-5f);
    VgSpatialColliderDesc d={0}; d.entity.value=31u; d.layer_mask=4u;
    d.transform=identity(); d.shape_type=VG_SPATIAL_SHAPE_STATIC_MESH;
    d.shape.mesh=mesh; d.enabled=true;
    VgSpatialCollider collider={0};
    CHECK(vg_spatial_collider_create(s,&d,&collider)==VG_OK);
    CHECK(vg_spatial_mesh_destroy(s,mesh)==VG_ERROR_CONFLICT);
    VgSpatialRayQuery ray={{-1,0,0},{1,0,0},2,4u,{0}};
    VgSpatialHit hit={0}; bool found=false;
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance-1.0f)<1e-3f);
    VgSpatialSphereSweep sweep={{-1,0,0},0.1f,{1,0,0},2,4u,{0}};
    CHECK(vg_spatial_sweep_sphere(s,&sweep,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance-0.9f)<1e-3f);
    VgSpatialStats before=vg_spatial_scene_stats(s);
    CHECK(before.triangle_tests>0u);
    const VgVec3 shifted[]={{2,-1,-1},{2,1,-1},{2,0,1}};
    VgSpatialMeshData replacement={shifted,3,indices,3};
    CHECK(vg_spatial_mesh_replace(s,mesh,&replacement)==VG_OK);
    visited=(MeshVisit){0};
    CHECK(vg_spatial_mesh_visit_triangles(s,mesh,visit_mesh,&visited)==VG_OK);
    CHECK(visited.count==1u && fabsf(visited.first_x-2.0f)<1e-5f);
    CHECK(vg_spatial_scene_stats(s).revision>before.revision);
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && !found);
    ray.max_distance=4;
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance-3.0f)<1e-3f);
    CHECK(vg_spatial_collider_destroy(s,collider)==VG_OK);
    CHECK(vg_spatial_mesh_destroy(s,mesh)==VG_OK);
    CHECK(vg_spatial_mesh_visit_triangles(s,mesh,visit_mesh,&visited)==VG_ERROR_INVALID_HANDLE);
}
static void test_shapes_and_parent(VgSpatialScene *s) {
    VgSpatialColliderDesc sphere={0}; sphere.entity.value=41u; sphere.layer_mask=2u;
    sphere.transform=identity(); sphere.shape_type=VG_SPATIAL_SHAPE_SPHERE;
    sphere.shape.sphere.radius=1; sphere.enabled=true;
    sphere.transform.scale=(VgVec3){2,2,2};
    VgSpatialCollider c={0}; CHECK(vg_spatial_collider_create(s,&sphere,&c)==VG_OK);
    VgSpatialSphereQuery overlap={{2.5f,0,0},0.5f,2u,{0}};
    bool found=false; VgSpatialHit hit={0};
    CHECK(vg_spatial_overlap_sphere(s,&overlap,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance)<1e-4f);
    sphere.transform.scale.y=3;
    CHECK(vg_spatial_collider_update(s,c,&sphere)==VG_ERROR_INVALID_ARGUMENT);
    CHECK(vg_spatial_collider_destroy(s,c)==VG_OK);

    VgSpatialColliderDesc capsule={0}; capsule.entity.value=42u; capsule.layer_mask=2u;
    capsule.transform=identity(); capsule.shape_type=VG_SPATIAL_SHAPE_CAPSULE;
    capsule.shape.capsule.point_a=(VgVec3){0,0,-1};
    capsule.shape.capsule.point_b=(VgVec3){0,0,1};
    capsule.shape.capsule.radius=0.5f; capsule.enabled=true;
    CHECK(vg_spatial_collider_create(s,&capsule,&c)==VG_OK);
    overlap.center=(VgVec3){0.75f,0,0}; overlap.radius=0.25f;
    CHECK(vg_spatial_overlap_sphere(s,&overlap,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance)<1e-4f);
    CHECK(vg_spatial_collider_destroy(s,c)==VG_OK);

    VgContext *context=NULL; VgWorld world={0}; VgEntity parent={0},child={0};
    VgContextDesc context_desc={0};
    context_desc.struct_size=sizeof(context_desc); context_desc.api_version=VG_API_VERSION;
    CHECK(vg_context_create(&context_desc,&context)==VG_OK);
    CHECK(vg_world_create(context,NULL,&world)==VG_OK);
    CHECK(vg_entity_create(context,world,&parent)==VG_OK);
    CHECK(vg_entity_create(context,world,&child)==VG_OK);
    VgTransform pt=identity(); pt.position=(VgVec3){4,0,0};
    pt.rotation=(VgQuat){0,0,0.70710678f,0.70710678f};
    CHECK(vg_entity_set_local_transform(context,parent,&pt)==VG_OK);
    CHECK(vg_entity_set_parent(context,child,parent,VG_REPARENT_KEEP_LOCAL)==VG_OK);
    VgTransform ct=identity(); ct.position=(VgVec3){1,0,0};
    CHECK(vg_entity_set_local_transform(context,child,&ct)==VG_OK);
    VgSpatialColliderDesc d=box_desc(child,(VgVec3){0,0,0},(VgVec3){0.25f,0.5f,1},8u);
    CHECK(vg_spatial_collider_create(s,&d,&c)==VG_OK);
    CHECK(vg_spatial_world_refresh_entity(context,s,c,&d)==VG_OK);
    VgSpatialRayQuery ray={{4,-2,0},{0,1,0},5,8u,{0}};
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance-2.75f)<1e-3f);
    pt.position.x=10;
    CHECK(vg_entity_set_local_transform(context,parent,&pt)==VG_OK);
    CHECK(vg_spatial_world_refresh_entity(context,s,c,&d)==VG_OK);
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && !found);
    CHECK(vg_spatial_collider_destroy(s,c)==VG_OK);
    CHECK(vg_world_destroy(context,world)==VG_OK); vg_context_destroy(context);
}
static void test_scene_isolation(void) {
    VgSpatialScene *a=NULL,*b=NULL;
    CHECK(vg_spatial_scene_create(NULL,&a)==VG_OK);
    CHECK(vg_spatial_scene_create(NULL,&b)==VG_OK);
    VgSpatialColliderDesc d=box_desc((VgEntity){1},(VgVec3){0},(VgVec3){1,1,1},1);
    VgSpatialCollider ca={0},cb={0};
    CHECK(vg_spatial_collider_create(a,&d,&ca)==VG_OK);
    CHECK(vg_spatial_collider_create(b,&d,&cb)==VG_OK);
    CHECK(ca.value!=cb.value);
    CHECK(vg_spatial_collider_destroy(b,ca)==VG_ERROR_INVALID_HANDLE);
    CHECK(vg_spatial_collider_destroy(a,ca)==VG_OK);
    CHECK(vg_spatial_collider_destroy(b,cb)==VG_OK);
    vg_spatial_scene_destroy(a); vg_spatial_scene_destroy(b);
}
static void test_long_mesh_sweep(VgSpatialScene *s) {
    const VgVec3 positions[]={
        {0,-2,0},{300,-2,0},{300,2,0},{0,2,0},
        {200,-2,0},{200,2,0},{200,2,3},{200,-2,3}};
    const uint32_t indices[]={0,1,2,0,2,3,4,5,6,4,6,7};
    VgSpatialMeshData data={positions,8,indices,12};
    VgSpatialMesh mesh={0}; CHECK(vg_spatial_mesh_create(s,&data,&mesh)==VG_OK);
    VgSpatialColliderDesc d={0}; d.layer_mask=16u; d.transform=identity();
    d.shape_type=VG_SPATIAL_SHAPE_STATIC_MESH; d.shape.mesh=mesh; d.enabled=true;
    VgSpatialCollider collider={0};
    CHECK(vg_spatial_collider_create(s,&d,&collider)==VG_OK);
    VgSpatialSphereSweep sweep={{0,0,1},0.1f,{1,0,0},300,16u,{0}};
    VgSpatialHit hit={0}; bool found=false;
    CHECK(vg_spatial_sweep_sphere(s,&sweep,&found,&hit)==VG_OK && found);
    CHECK(fabsf(hit.distance-199.9f)<0.01f && hit.normal.x < -0.99f);
    CHECK(vg_spatial_collider_destroy(s,collider)==VG_OK);
    CHECK(vg_spatial_mesh_destroy(s,mesh)==VG_OK);
}
static void test_shallow_box_approach(VgSpatialScene *s) {
    VgSpatialColliderDesc d=box_desc((VgEntity){52},(VgVec3){0},
                                     (VgVec3){500,1,1},32u);
    VgSpatialCollider collider={0};
    CHECK(vg_spatial_collider_create(s,&d,&collider)==VG_OK);
    VgSpatialSphereSweep sweep={{-100,-1.1001f,0},0.1f,
                                {1,0.000001f,0},200,32u,{0}};
    VgSpatialHit hit={0}; bool found=false;
    CHECK(vg_spatial_sweep_sphere(s,&sweep,&found,&hit)==VG_OK && found);
    CHECK(hit.distance>85.0f && hit.distance<95.0f && hit.normal.y < -0.99f);
    sweep.direction=(VgVec3){1,-0.000001f,0};
    CHECK(vg_spatial_sweep_sphere(s,&sweep,&found,&hit)==VG_OK && !found);
    CHECK(vg_spatial_collider_destroy(s,collider)==VG_OK);
}
static void test_validation_and_capacity(void) {
    VgSpatialSceneConfig config={0}; config.max_meshes=1u;
    config.max_colliders=1u; config.max_triangles_per_mesh=1u;
    VgSpatialScene *s=NULL; CHECK(vg_spatial_scene_create(&config,&s)==VG_OK);
    const VgVec3 positions[]={{0,0,0},{1,0,0},{0,1,0}};
    const uint32_t bad_indices[]={0,1,3};
    VgSpatialMeshData bad={positions,3,bad_indices,3}; VgSpatialMesh mesh={0};
    CHECK(vg_spatial_mesh_create(s,&bad,&mesh)==VG_ERROR_INVALID_ARGUMENT);
    const uint32_t indices[]={0,1,2};
    VgSpatialMeshData valid={positions,3,indices,3};
    CHECK(vg_spatial_mesh_create(s,&valid,&mesh)==VG_OK);
    VgSpatialMesh other={0}; CHECK(vg_spatial_mesh_create(s,&valid,&other)==VG_ERROR_CAPACITY);
    VgSpatialColliderDesc d=box_desc((VgEntity){1},(VgVec3){0},(VgVec3){1,1,1},1);
    VgSpatialCollider c={0}; CHECK(vg_spatial_collider_create(s,&d,&c)==VG_OK);
    d.transform.scale.x=0;
    CHECK(vg_spatial_collider_update(s,c,&d)==VG_ERROR_INVALID_ARGUMENT);
    VgSpatialRayQuery ray={{-2,0,0},{1,0,0},4,1,{0}};
    bool found=false; VgSpatialHit hit={0};
    CHECK(vg_spatial_raycast(s,&ray,&found,&hit)==VG_OK && found);
    d.transform.scale.x=1;
    CHECK(vg_spatial_collider_create(s,&d,&(VgSpatialCollider){0})==VG_ERROR_CAPACITY);
    CHECK(vg_spatial_collider_destroy(s,c)==VG_OK);
    CHECK(vg_spatial_mesh_destroy(s,mesh)==VG_OK);
    vg_spatial_scene_destroy(s);
}
int main(void) {
    VgSpatialScene *scene=NULL;
    CHECK(vg_spatial_scene_create(NULL,&scene)==VG_OK);
    test_box_and_filters(scene);
    test_mesh_and_revision(scene);
    test_shapes_and_parent(scene);
    test_long_mesh_sweep(scene);
    test_shallow_box_approach(scene);
    VgSpatialStats stats=vg_spatial_scene_stats(scene);
    CHECK(stats.colliders==0u && stats.meshes==0u);
    vg_spatial_scene_destroy(scene);
    test_scene_isolation();
    test_validation_and_capacity();
    puts("spatial: all cases passed"); return 0;
}
