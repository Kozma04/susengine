#pragma once

#define COLLIDER_MAX_CONTACTS 4
#define PHYSICS_WORLD_MAX_CONTACTS 128

#include <stdint.h>
#include <raylib.h>
#include <raymath.h>
#include <float.h>

#include "./dsa.h"
#include "./logger.h"
#include "./gjk.h"


typedef struct ColliderMesh {
    float *vertices; // set of (X, Y, Z) coordinates
    size_t nVertices; // number of coordinates (vertices size = nVertices * 3)
    short *indices; // only used in raycasting to define mesh triangles
} ColliderMesh;


typedef enum ColliderTypeEnum {
    COLLIDER_TYPE_SPHERE,
    COLLIDER_TYPE_AABB,
    COLLIDER_TYPE_CONVEX_HULL
} ColliderType;

typedef struct RigidBody {
    uint8_t enableDynamics;

    Vector3 pos;
    Vector3 vel;
    Vector3 accel;
    Vector3 gravity;

    Vector3 angularVel;
    Vector3 inverseInertia;
    Matrix inverseInertiaTensor;
    Quaternion rot;
    Vector3 enableRot;

    float mediumFriction;
    float mass;
    float bounce;
    float staticFriction;
    float dynamicFriction;
} RigidBody;

typedef struct ColliderContact {
    Vector3 pointA;
    Vector3 pointB;
    Vector3 normal;
    float depth;
    uint32_t sourceId;
    uint32_t targetId;
} ColliderContact;

typedef struct ColliderRayContact {
    uint32_t id;
    float dist;
    Vector3 normal;
} ColliderRayContact;

typedef struct Collider {
    ColliderType type;
    ColliderContact contacts[COLLIDER_MAX_CONTACTS];
    size_t nContacts;
    uint32_t collMask;
    uint32_t collTargetMask;
    BoundingBox bounds;
    BoundingBox _boundsTransformed;
    Matrix localTransform;
    union {
        struct {
            float radius;
        } sphere;
        ColliderMesh convexHull;
    };
} Collider;

/*typedef struct PhysicsSystemEntity {
    PhysicsRigidBody *body;
    Collider *collider;
    Matrix *transform;
    Matrix transformInverse;
} PhysicsSystemEntity;*/

typedef struct ColliderEntity {
    Collider *coll;
    Matrix *transform;
    Matrix transformInverse;
} ColliderEntity;

typedef struct PhysicsSystem {
    // A body's Collider and PhysicsSystemEntity share the same index
    Hashmap collEnt; // ColliderEntity*
    Hashmap rigidBodies; // PhysicsRigidBody*

    struct {
        int posA, posB;
        Vector3 pointA, pointB;
        Vector3 normal;
        float depth;
    } contacts[PHYSICS_WORLD_MAX_CONTACTS];
    size_t nContacts;

    struct {
        int posA, posB;
    } contactsBroad[PHYSICS_WORLD_MAX_CONTACTS];
    size_t nContactsBroad;

    struct {
        float penetrationOffset;
        float penetrationScale;
    } correction;
} PhysicsSystem;

PhysicsSystem physics_initSystem();
RigidBody physics_initRigidBody(float mass);

void physics_addCollider(
    PhysicsSystem *sys, uint32_t id, Collider *coll, Matrix *transform
);
void physics_addRigidBody(
    PhysicsSystem *sys, uint32_t id, RigidBody *rb
);
void physics_removeCollider(PhysicsSystem *sys, uint32_t id);
void physics_removeRigidBody(PhysicsSystem *sys, uint32_t id);

void physics_setPosition(RigidBody *rb, Vector3 pos);
void physics_applyForce(RigidBody *rb, Vector3 force);
void physics_applyAngularImpulse(RigidBody *rb, Vector3 force);

void physics_raycast(
    PhysicsSystem *sys, Ray ray, float maxDist, ColliderRayContact *cont,
    size_t *nCont, size_t maxCont, uint32_t mask
);
void physics_updateCollisions(PhysicsSystem *sys);
void physics_updateBodies(PhysicsSystem *sys, float dt);