#pragma once

#define COLLIDER_MAX_CONTACTS 4
#define PHYSICS_WORLD_MAX_CONTACTS 128

#include <stdint.h>
#include <raylib.h>
#include <raymath.h>
#include <float.h>

#include "dsa.h"
#include "logger.h"
#include "gjk.h"


typedef struct ColliderMesh {
    float *vertices; // set of (X, Y, Z) coordinates
    size_t nVertices; // number of coordinates (vertices size = nVertices * 3)
} ColliderMesh;


typedef enum ColliderTypeEnum {
    COLLIDER_TYPE_SPHERE,
    COLLIDER_TYPE_AABB,
    COLLIDER_TYPE_CONVEX_HULL
} ColliderType;

typedef struct PhysicsRigidBody {
    Vector3 pos;
    Vector3 vel;
    Vector3 accel;
    Vector3 gravity;
    float mediumFriction;
    float mass;
    float bounce;
    float staticFriction;
    float dynamicFriction;
} PhysicsRigidBody;

typedef struct ColliderContact {
    Vector3 normal;
    float depth;
    uint32_t sourceId;
    uint32_t targetId;
} ColliderContact;

typedef struct Collider {
    ColliderType type;
    ColliderContact contacts[COLLIDER_MAX_CONTACTS];
    size_t nContacts;
    uint32_t collMask;
    uint32_t collTargetMask;
    union {
        struct {
            float radius;
        } sphere;
        ColliderMesh convexHull;
    };
} Collider;

typedef struct PhysicsSystemEntity {
    PhysicsRigidBody *body;
    Collider *collider;
    Matrix *transform;
    Matrix transformInverse;
} PhysicsSystemEntity;

typedef struct PhysicsSystem {
    Hashmap sysEntities;

    struct {
        PhysicsRigidBody *bodyA, *bodyB;
        Vector3 normal;
        float depth;
    } contacts[PHYSICS_WORLD_MAX_CONTACTS];
    size_t nContacts;
} PhysicsSystem;

PhysicsSystem physics_initSystem();
PhysicsRigidBody physics_initRigidBody(float mass);

void physics_addRigidBody(
    PhysicsSystem *sys, uint32_t id, PhysicsRigidBody *rb,
    Collider *coll, Matrix *transform
);
void physics_removeRigidBody(PhysicsSystem *sys, uint32_t id);

void physics_setPosition(PhysicsRigidBody *rb, Vector3 pos);

void physics_updateSystem(PhysicsSystem *sys, float dt);