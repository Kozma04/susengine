#pragma once

#define PHYSICS_MAX_CONTACTS 4

#include <stdint.h>
#include <raylib.h>
#include <raymath.h>
#include <float.h>

#include "dsa.h"
#include "logger.h"


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
    Vector3 oldPos;
    Vector3 accel;
    float mass;
} PhysicsRigidBody;

typedef struct ColliderContact {
    Vector3 normal;
    float depth;
    uint32_t targetId;
} ColliderContact;

typedef struct Collider {
    ColliderType type;
    ColliderContact contacts[PHYSICS_MAX_CONTACTS];
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
} PhysicsSystemEntity;

typedef struct PhysicsSystem {
    Hashmap sysEntities;  
} PhysicsSystem;


PhysicsSystem physics_initSystem();
PhysicsRigidBody physics_initRigidBody(float mass);

void physics_addRigidBody(
    PhysicsSystem *sys, uint32_t id, PhysicsRigidBody *rb,
    Collider *coll, Matrix *transform
);
void physics_removeRigidBody(PhysicsSystem *sys, uint32_t id);

void physics_updateSystem(PhysicsSystem *sys, float dt);

uint8_t computeGJK(ColliderMesh meshA, ColliderMesh meshB, Matrix transformA, Matrix transformB);