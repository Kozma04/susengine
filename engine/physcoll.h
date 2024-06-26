#pragma once

#define COLLIDER_MAX_CONTACTS 4
#define PHYSICS_WORLD_MAX_CONTACTS 128

#include <float.h>
#include <raylib.h>
#include <raymath.h>
#include <stdint.h>

#include "./dsa.h"
#include "./gjk.h"
#include "./logger.h"

typedef struct ColliderMesh {
    float *vertices;  // set of (X, Y, Z) coordinates
    size_t nVertices; // number of coordinates (vertices size = nVertices * 3)
    unsigned short *indices; // only used in raycasting to define mesh triangles
} ColliderMesh;

typedef struct ColliderSphere {
    float radius;
} ColliderSphere;

typedef struct ColliderHeightmap {
    float *map;
    uint16_t sizeX;
    uint16_t sizeY;
} ColliderHeightmap;

typedef enum ColliderTypeEnum {
    COLLIDER_TYPE_SPHERE,
    COLLIDER_TYPE_AABB,
    COLLIDER_TYPE_CONVEX_HULL,
    COLLIDER_TYPE_HEIGHTMAP,
    COLLIDER_TOTAL_TYPES
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
    Vector3 cog; // center of gravity

    uint32_t _idleAngularVelTicks;
    uint32_t _idleVelTicks;
    float _avgVelLen;
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
    uint8_t enabled;
    ColliderType type;
    ColliderContact *contacts;
    size_t nContacts;
    uint32_t collMask;
    uint32_t collTargetMask;
    BoundingBox bounds;
    BoundingBox _boundsTransformed;
    Matrix localTransform;
    union {
        ColliderSphere sphere;
        ColliderMesh convexHull;
        ColliderHeightmap heightmap;
    };
} Collider;

typedef struct ColliderEntity {
    Collider *coll;
    Matrix *transform;
    Matrix transformInverse;
} ColliderEntity;

typedef enum JointTypeEnum { JOINT_TYPE_RIGID, JOINT_TYPE_ELASTIC } JointType;

typedef struct Joint {
    uint8_t enabled;
    JointType type;
} Joint;

typedef struct PhysicsSystem {
    // A body's Collider and PhysicsSystemEntity share the same index
    Hashmap collEnt;     // ColliderEntity*
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
        // Impulse resolution
        float penetrationOffset;
        float penetrationScale;
        // Angular velocity
        float angularVelDamping;
        // Idle state (no pos/rot updates) conditions
        float idleVelThres;
        float idleAngVelThres;
        uint32_t idleVelTicks;
        uint32_t idleAngVelTicks;
    } correction;
} PhysicsSystem;

Collider initCollider();
PhysicsSystem physics_initSystem();
RigidBody physics_initRigidBody(float mass);

void physics_addCollider(PhysicsSystem *sys, uint32_t id, Collider *coll,
                         Matrix *transform);
void physics_addRigidBody(PhysicsSystem *sys, uint32_t id, RigidBody *rb);
void physics_removeCollider(PhysicsSystem *sys, uint32_t id);
void physics_removeRigidBody(PhysicsSystem *sys, uint32_t id);

void physics_setPosition(RigidBody *rb, Vector3 pos);
void physics_applyForce(RigidBody *rb, Vector3 force);
void physics_applyAngularImpulse(RigidBody *rb, Vector3 force);
void physics_applyImpulse(RigidBody *rb, Vector3 impulse);
void physics_applyImpulseAt(RigidBody *rb, Vector3 impulse, Vector3 relPos);

void physics_raycast(PhysicsSystem *sys, Ray ray, float maxDist,
                     ColliderRayContact *cont, size_t *nCont, size_t maxCont,
                     uint32_t mask);
void physics_updateCollisions(PhysicsSystem *sys);
void physics_updateBodies(PhysicsSystem *sys, float dt);