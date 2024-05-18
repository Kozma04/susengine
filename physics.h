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

    Vector3 angularVel;
    //Vector3 torque;
    Vector3 inverseInertia;
    Matrix inverseInertiaTensor;
    Quaternion rot;

    float mediumFriction;
    float mass;
    float bounce;
    float staticFriction;
    float dynamicFriction;
    
    //uint8_t enableAngularVel;
    Vector3 enableRot;
} PhysicsRigidBody;

typedef struct ColliderContact {
    Vector3 pointA;
    Vector3 pointB;
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
    BoundingBox bounds;
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
        Vector3 pointA, pointB;
        Vector3 normal;
        float depth;
    } contacts[PHYSICS_WORLD_MAX_CONTACTS];
    size_t nContacts;

    struct {
        int bodyIdxA, bodyIdxB;
    } contactsBroad[PHYSICS_WORLD_MAX_CONTACTS];
    size_t nContactsBroad;
} PhysicsSystem;

PhysicsSystem physics_initSystem();
PhysicsRigidBody physics_initRigidBody(float mass);

void physics_addRigidBody(
    PhysicsSystem *sys, uint32_t id, PhysicsRigidBody *rb,
    Collider *coll, Matrix *transform
);
void physics_removeRigidBody(PhysicsSystem *sys, uint32_t id);

void physics_setPosition(PhysicsRigidBody *rb, Vector3 pos);
void physics_applyAngularImpulse(PhysicsRigidBody *rb, Vector3 force);

void physics_updateSystem(PhysicsSystem *sys, float dt);