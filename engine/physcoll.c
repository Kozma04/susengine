#include "./physcoll.h"
#include "mathutils.h"
#include <raymath.h>

typedef uint8_t (*CollSolverFn)(ColliderEntity *entA, ColliderEntity *entB,
                                Vector3 *nor, Vector3 *locA, Vector3 *locB);

Collider initCollider() {
    Collider coll;
    coll.enabled = 1;
    coll.collMask = 0xffffffff;
    coll.collTargetMask = 0;
    coll.contacts = malloc(COLLIDER_MAX_CONTACTS * sizeof(*coll.contacts));
    coll.nContacts = 0;
    coll.type = COLLIDER_TOTAL_TYPES;
    coll.localTransform = MatrixIdentity();
    return coll;
}

RigidBody physics_initRigidBody(float mass) {
    RigidBody res;
    res.enableDynamics = 1;
    res.vel = (Vector3){0, 0, 0};
    res.accel = (Vector3){0, 0, 0};
    res.pos = (Vector3){0, 0, 0};
    res.gravity = (Vector3){0, -9.8f, 0};

    res.angularVel = (Vector3){0, 0, 0};
    res.inverseInertia = (Vector3){.5f, .5f, .5f};
    res.inverseInertiaTensor = MatrixIdentity();
    res.rot = QuaternionIdentity();

    res.mass = mass;
    res.bounce = .0f;
    res.staticFriction = 0.1f;
    res.dynamicFriction = 0.5f;
    res.mediumFriction = 0.001f;

    res.enableRot = (Vector3){1, 1, 1};
    res._idleVelTicks = 0;
    res._idleAngularVelTicks = 0;
    res._avgVelLen = 0;
    return res;
}

PhysicsSystem physics_initSystem() {
    PhysicsSystem sys;
    sys.collEnt = hashmap_init();
    sys.rigidBodies = hashmap_init();
    sys.nContacts = 0;
    // sys.nContactsBroad = 0;
    sys.correction.penetrationOffset = 0.01f; // 0.0002f;
    sys.correction.penetrationScale = 0.3f;   // 0.005f;
    sys.correction.idleAngVelThres = 0.6f;
    sys.correction.idleAngVelTicks = 60 * 10;
    sys.correction.idleVelThres = 0.3f;
    sys.correction.idleVelTicks = 60 * 5;
    sys.correction.angularVelDamping = 2.8f;
    return sys;
}

void physics_addCollider(PhysicsSystem *sys, uint32_t id, Collider *coll,
                         Matrix *transform) {
    ColliderEntity *ent = malloc(sizeof(*ent));
    ent->coll = coll;
    ent->transform = transform;
    ent->transformInverse = MatrixIdentity();
    hashmap_set(&sys->collEnt, id, (HashmapVal)(void *)ent);

    logMsg(LOG_LVL_DEBUG, "added collider id %u to physics system", id);
}

void physics_addRigidBody(PhysicsSystem *sys, uint32_t id, RigidBody *rb) {
    hashmap_set(&sys->rigidBodies, id, (HashmapVal)(void *)rb);
    logMsg(LOG_LVL_DEBUG, "added rigidbody id %u to physics system", id);
}

void physics_removeCollider(PhysicsSystem *sys, uint32_t id) {
    ColliderEntity *ent;
    if (!hashmap_getP(&sys->collEnt, id, (void **)&ent)) {
        logMsg(LOG_LVL_ERR, "collider id %u not found in physics system", id);
        return;
    }
    free(ent);
    hashmap_del(&sys->collEnt, id, 0);
    logMsg(LOG_LVL_DEBUG, "removed collider id %u from physics system", id);
}

void physics_removeRigidBody(PhysicsSystem *sys, uint32_t id) {
    hashmap_del(&sys->rigidBodies, id, 0);
}

void physics_setPosition(RigidBody *rb, Vector3 pos) { rb->pos = pos; }

void physics_applyForce(RigidBody *rb, Vector3 force) {
    if (rb->mass == 0.f)
        return;
    rb->accel = Vector3Add(rb->accel, Vector3Scale(force, 1.f / rb->mass));
}

void physics_applyAngularImpulse(RigidBody *rb, Vector3 force) {
    if (rb->mass == 0.f)
        return;
    rb->angularVel = Vector3Add(
        rb->angularVel, Vector3Transform(force, rb->inverseInertiaTensor));
}
void physics_applyImpulse(RigidBody *rb, Vector3 impulse) {
    if (rb->mass == 0.f)
        return;
    impulse = Vector3Scale(impulse, 1.f / rb->mass);
    rb->vel = Vector3Add(rb->vel, impulse);
}

void physics_applyImpulseAt(RigidBody *rb, Vector3 impulse, Vector3 relPos) {
    if (rb->mass == 0.f)
        return;
    physics_applyImpulse(rb, impulse);
    physics_applyAngularImpulse(rb, Vector3CrossProduct(relPos, impulse));
}

static void physics_bodyUpdate(PhysicsSystem *sys, RigidBody *body, float dt) {
    if (!body->enableDynamics)
        return;

    Vector3 totalAccel = body->accel;
    uint8_t idle = 0;

    if (body->mass != .0f)
        totalAccel = Vector3Add(totalAccel, body->gravity);

    const float waterLevel = -5.f + sin(GetTime()) + 1;
    if (body->pos.y < waterLevel && body->mass != 0.f) {
        float depth = waterLevel - body->pos.y + 1 + 3;
        totalAccel = Vector3Add(totalAccel, (Vector3){0, depth * 3, 0});
        body->vel = Vector3Scale(body->vel, 0.98);
    }

    body->vel = Vector3Add(body->vel, Vector3Scale(totalAccel, dt));
    body->vel = Vector3Scale(body->vel, 1.f - body->mediumFriction * dt);

    idle = 0;
    body->_avgVelLen +=
        (Vector3LengthSqr(body->vel) - body->_avgVelLen) * 4.f * dt;
    if (body->_avgVelLen < powf(sys->correction.idleVelThres, 2.f)) {
        body->_idleVelTicks++;
        if (body->_idleVelTicks >= sys->correction.idleVelTicks) {
            // body->vel = (Vector3){0, 0, 0};
            idle = 1;
        }
    } else {
        if (body->_idleVelTicks >= sys->correction.idleVelTicks)
            body->vel = (Vector3){0, 0, 0};
        body->_idleVelTicks = 0;
    }
    if (!idle)
        body->pos = Vector3Add(body->pos, Vector3Scale(body->vel, dt));

    // Update inverse inertia tensor
    Vector3 invIn = (Vector3){0, 0, 0};
    if (body->mass != .0f)
        invIn = Vector3Scale(body->inverseInertia, 1.f / body->mass);

    Quaternion rotQConj =
        (Quaternion){-body->rot.x, -body->rot.y, -body->rot.z, body->rot.w};
    Matrix invRot = QuaternionToMatrix(rotQConj);
    Matrix rot = QuaternionToMatrix(body->rot);

    body->inverseInertiaTensor =
        MatrixMultiply(rot, MatrixScale(invIn.x, invIn.y, invIn.z));
    body->inverseInertiaTensor =
        MatrixMultiply(body->inverseInertiaTensor, invRot);

    // Apply rotation
    idle = 0;
    if (Vector3LengthSqr(Vector3Multiply(body->angularVel, body->enableRot)) <
        powf(sys->correction.idleAngVelThres, 2.f)) {
        body->_idleAngularVelTicks++;
        if (body->_idleAngularVelTicks >= sys->correction.idleAngVelTicks) {
            body->angularVel = (Vector3){0, 0, 0};
            idle = 1;
        }
    } else {
        body->_idleAngularVelTicks = 0;
    }
    if (!idle) {
        // Update orientation
        Quaternion angVelQ = (Quaternion){
            body->angularVel.x * dt * 0.5f * body->enableRot.x,
            body->angularVel.y * dt * 0.5f * body->enableRot.y,
            body->angularVel.z * dt * 0.5f * body->enableRot.z, 0.f};
        body->rot =
            QuaternionAdd(body->rot, QuaternionMultiply(angVelQ, body->rot));
        body->rot = QuaternionNormalize(body->rot);

        // Angular velocity damping
        body->angularVel = Vector3Scale(
            body->angularVel, 1.f - sys->correction.angularVelDamping * dt);
    }
}

static Vector3 getMatrixTranslation(Matrix *mat) {
    return (Vector3){mat->m12, mat->m13, mat->m14};
}

static void physics_collisionBroadPhase(PhysicsSystem *sys) {
    const ColliderEntity *entA, *entB;
    BoundingBox bbA, bbB;
    uint32_t maskA, maskB;

    for (size_t i = 0; i < sys->collEnt.nEntries; i++) {
        entA = (const ColliderEntity *)sys->collEnt.entries[i].val.ptr;
        entA->coll->_boundsTransformed = BoxTransform(
            entA->coll->bounds,
            MatrixMultiply(entA->coll->localTransform, *entA->transform));
    }

    sys->nContactsBroad = 0;
    for (size_t i = 0; i < sys->collEnt.nEntries; i++) {
        entA = (const ColliderEntity *)sys->collEnt.entries[i].val.ptr;
        bbA = entA->coll->_boundsTransformed;

        for (size_t j = i + 1; j < sys->collEnt.nEntries; j++) {
            entB = (const ColliderEntity *)sys->collEnt.entries[j].val.ptr;
            bbB = entB->coll->_boundsTransformed;
            maskA = entA->coll->collTargetMask;
            maskB = entB->coll->collMask;

            if (!entA->coll->enabled || !entB->coll->enabled)
                continue;
            if ((maskA & maskB) != maskA)
                continue;
            if (BoxIntersect(bbA, bbB)) {
                if (sys->nContactsBroad >= PHYSICS_WORLD_MAX_CONTACTS) {
                    logMsg(LOG_LVL_ERR, "too many AABB contacts in system");
                    return;
                }
                sys->contactsBroad[sys->nContactsBroad].posA = i;
                sys->contactsBroad[sys->nContactsBroad].posB = j;
                sys->nContactsBroad++;
            }
        }
    }
}

static GJKColliderMesh genGJKMesh(ColliderEntity *ent) {
    GJKColliderMesh gjkMesh;
    gjkMesh.vertices = ent->coll->convexHull.vertices;
    gjkMesh.nVertices = ent->coll->convexHull.nVertices;
    gjkMesh.pos = Vector3Add(getMatrixTranslation(ent->transform),
                             getMatrixTranslation(&ent->coll->localTransform));
    // gjkMesh.pos = getMatrixTranslation(ent->transform);
    gjkMesh.transform = *ent->transform;
    gjkMesh.transform =
        MatrixMultiply(ent->coll->localTransform, gjkMesh.transform);
    gjkMesh.transform.m12 = 0;
    gjkMesh.transform.m13 = 0;
    gjkMesh.transform.m14 = 0;
    gjkMesh.transformInverse = MatrixInvert(gjkMesh.transform);
    return gjkMesh;
}

static uint8_t solveCollNull(ColliderEntity *entA, ColliderEntity *entB,
                             Vector3 *nor, Vector3 *locA, Vector3 *locB) {
    logMsg(LOG_LVL_ERR, "only convex hull vs convex hull supported");
    return 0;
}

static uint8_t solveCollConvHull(ColliderEntity *entA, ColliderEntity *entB,
                                 Vector3 *nor, Vector3 *locA, Vector3 *locB) {
    GJKColliderMesh meshA, meshB;
    meshA = genGJKMesh(entA);
    meshB = genGJKMesh(entB);
    return gjk(&meshA, &meshB, nor, locA, locB);
}

static uint8_t solveCollConvHullHeightmap(ColliderEntity *entA,
                                          ColliderEntity *entB, Vector3 *nor,
                                          Vector3 *locA, Vector3 *locB) {
    const ColliderType typeA = entA->coll->type;
    ColliderEntity *entHMap = typeA == COLLIDER_TYPE_CONVEX_HULL ? entB : entA;
    ColliderEntity *entHull = typeA == COLLIDER_TYPE_CONVEX_HULL ? entA : entB;
    ColliderEntity entTmpHMap = *entHMap;
    GJKColliderMesh meshHMap;
    GJKColliderMesh meshHull = genGJKMesh(entHull);
    ColliderHeightmap *hMap = &entHMap->coll->heightmap;
    BoundingBox cellBB;

    // Terrain cell convex hull
    Vector3 va, vb, vc;
    Collider tmpColl = *entHMap->coll;
    float tmpVert[3 * 6];

    // For building terrain cell convex hull
    uint8_t collide = 0;
    float maxNorLenSqr = 0;
    Vector3 tmpNor, tmpLocA, tmpLocB;

    //int statTests = 0;

    // clang-format off
    static short tmpInd[] = {
        0, 1, 2,  3, 4, 5,
        3, 4, 1,  3, 1, 0,
        5, 3, 0,  5, 0, 2,
        4, 5, 2,  2, 1, 4
    };
    // clang-format on

    tmpColl.type = COLLIDER_TYPE_CONVEX_HULL;
    tmpColl.convexHull.vertices = tmpVert;
    tmpColl.convexHull.indices = tmpInd;
    tmpColl.convexHull.nVertices = 6;

    entTmpHMap.coll = &tmpColl;
    meshHMap = genGJKMesh(&entTmpHMap);

    if (hMap->sizeX < 2 || hMap->sizeY < 2) {
        logMsg(LOG_LVL_ERR, "invalid heightmap size: %u x %u", hMap->sizeX,
               hMap->sizeY);
        return 0;
    }

    for (int x = 0; x < hMap->sizeX - 1; x++) {
        for (int z = 0; z < hMap->sizeY - 1; z++) {
            float hA = hMap->map[z * hMap->sizeX + x];
            float hB = hMap->map[z * hMap->sizeX + x + 1];
            float hC = hMap->map[(z + 1) * hMap->sizeX + x];
            float hD = hMap->map[(z + 1) * hMap->sizeX + x + 1];
            float hMax = hA;
            if (hB > hMax)
                hMax = hB;
            if (hC > hMax)
                hMax = hC;
            if (hD > hMax)
                hMax = hD;

            // Generate terrain cell bounding box
            cellBB.min.x = x;
            cellBB.min.y = 0;
            cellBB.min.z = z;
            cellBB.max.x = x + 1;
            cellBB.max.y = hMax;
            cellBB.max.z = z + 1;
            cellBB = BoxTransform(cellBB, *entHMap->transform);

            // Discard cell if bounding boxes do not intersect
            if (!BoxIntersect(cellBB, entHull->coll->_boundsTransformed))
                continue;
            //statTests++;

            // Check first triangle in cell
            // Top
            tmpVert[0 + 0] = x;
            tmpVert[0 + 1] = hA;
            tmpVert[0 + 2] = z;
            tmpVert[3 + 0] = x + 1;
            tmpVert[3 + 1] = hB;
            tmpVert[3 + 2] = z;
            tmpVert[6 + 0] = x;
            tmpVert[6 + 1] = hC;
            tmpVert[6 + 2] = z + 1;
            // Bottom
            tmpVert[9 + 0] = x;
            tmpVert[9 + 1] = 0;
            tmpVert[9 + 2] = z;
            tmpVert[12 + 0] = x + 1;
            tmpVert[12 + 1] = 0;
            tmpVert[12 + 2] = z;
            tmpVert[15 + 0] = x;
            tmpVert[15 + 1] = 0;
            tmpVert[15 + 2] = z + 1;
            if (gjk(&meshHull, &meshHMap, &tmpNor, &tmpLocA, &tmpLocB)) {
                float len = Vector3LengthSqr(tmpNor);
                collide = 1;
                if (len > maxNorLenSqr) {
                    maxNorLenSqr = len;
                    *nor = tmpNor;
                    *locA = tmpLocA;
                    *locB = tmpLocB;
                }
            }

            // Check second triangle in cell
            // Top
            tmpVert[0 + 0] = x + 1;
            tmpVert[0 + 1] = hB;
            tmpVert[0 + 2] = z;
            tmpVert[3 + 0] = x + 1;
            tmpVert[3 + 1] = hD;
            tmpVert[3 + 2] = z + 1;
            tmpVert[6 + 0] = x;
            tmpVert[6 + 1] = hC;
            tmpVert[6 + 2] = z + 1;
            // Bottom
            tmpVert[9 + 0] = x + 1;
            tmpVert[9 + 1] = 0;
            tmpVert[9 + 2] = z;
            tmpVert[12 + 0] = x + 1;
            tmpVert[12 + 1] = 0;
            tmpVert[12 + 2] = z + 1;
            tmpVert[15 + 0] = x;
            tmpVert[15 + 1] = 0;
            tmpVert[15 + 2] = z + 1;

            if (gjk(&meshHull, &meshHMap, &tmpNor, &tmpLocA, &tmpLocB)) {
                float len = Vector3LengthSqr(tmpNor);
                collide = 1;
                if (len > maxNorLenSqr) {
                    maxNorLenSqr = len;
                    *nor = tmpNor;
                    *locA = tmpLocA;
                    *locB = tmpLocB;
                }
            }
        }
    }

    //logMsg(LOG_LVL_INFO, "performed %u tests!", statTests);

    return collide;
}

// clang-format off
static const CollSolverFn collisionSolvers[COLLIDER_TOTAL_TYPES][COLLIDER_TOTAL_TYPES] = {
    {solveCollNull, solveCollNull, solveCollNull, solveCollNull},
    {solveCollNull, solveCollNull, solveCollNull, solveCollNull},
    {solveCollNull, solveCollNull, solveCollConvHull, solveCollConvHullHeightmap},
    {solveCollNull, solveCollNull, solveCollConvHullHeightmap, solveCollNull}
};
// clang-format on

static void physics_collisionNarrowPhase(PhysicsSystem *sys) {
    ColliderEntity *entA, *entB;
    ColliderContact cont;
    Vector3 nor, locA, locB;
    int idxA, idxB;

    for (size_t i = 0; i < sys->collEnt.nEntries; i++) {
        entA = (ColliderEntity *)sys->collEnt.entries[i].val.ptr;
        // collA = entA->coll;
        entA->coll->nContacts = 0;
        entA->transformInverse = MatrixInvert(
            MatrixMultiply(*entA->transform, entA->coll->localTransform));
    }

    sys->nContacts = 0;
    for (size_t i = 0; i < sys->nContactsBroad; i++) {
        idxA = sys->contactsBroad[i].posA;
        idxB = sys->contactsBroad[i].posB;
        entA = sys->collEnt.entries[idxA].val.ptr;
        entB = sys->collEnt.entries[idxB].val.ptr;

        if (entA->coll->type >= COLLIDER_TOTAL_TYPES) {
            logMsg(LOG_LVL_ERR, "invalid collider type for id %u: %u",
                   sys->collEnt.entries[idxA].key, entA->coll->type);
            continue;
        }
        if (entB->coll->type >= COLLIDER_TOTAL_TYPES) {
            logMsg(LOG_LVL_ERR, "invalid collider type for id %u: %u",
                   sys->collEnt.entries[idxB].key, entB->coll->type);
            continue;
        }
        // uint8_t collRes = gjk(&gjkMeshA, &gjkMeshB, &nor, &locA, &locB);
        CollSolverFn solveCollision =
            collisionSolvers[entA->coll->type][entB->coll->type];
        uint8_t collRes = solveCollision(entA, entB, &nor, &locA, &locB);

        if (collRes) {
            if (isnan(nor.x) || isnan(nor.y) || isnan(nor.z)) {
                printf("nor: %g %g %g\n", nor.x, nor.y, nor.z);
                continue;
            }
            if (isnan(locA.x) || isnan(locA.y) || isnan(locA.z)) {
                printf("locA: %g %g %g\n", locA.x, locA.y, locA.z);
                continue;
            }
            if (isnan(locB.x) || isnan(locB.y) || isnan(locB.z)) {
                printf("locB: %g %g %g\n", locB.x, locB.y, locB.z);
                continue;
            }

            cont.depth = Vector3Length(nor) * .5f;
            cont.normal = Vector3Normalize(nor);

            if (sys->nContacts < PHYSICS_WORLD_MAX_CONTACTS) {
                sys->contacts[sys->nContacts].posA = idxA;
                sys->contacts[sys->nContacts].posB = idxB;
                sys->contacts[sys->nContacts].depth = cont.depth;
                sys->contacts[sys->nContacts].normal = cont.normal;
                sys->contacts[sys->nContacts].pointA = locA;
                sys->contacts[sys->nContacts].pointB = locB;
                sys->nContacts++;
            } else
                logMsg(LOG_LVL_ERR, "too many collisions in system");

            if (entA->coll->nContacts < COLLIDER_MAX_CONTACTS) {
                cont.sourceId = sys->collEnt.entries[idxA].key;
                cont.targetId = sys->collEnt.entries[idxB].key;
                cont.pointA = locA;
                cont.pointB = locB;
                entA->coll->contacts[entA->coll->nContacts++] = cont;
            } else {
                /*logMsg(
                    LOG_LVL_WARN, "too many collisions for collider %u",
                    sys->sysEntities.entries[i].key
                );*/
            }
            if (entB->coll->nContacts < COLLIDER_MAX_CONTACTS) {
                cont.sourceId = sys->collEnt.entries[idxB].key;
                cont.targetId = sys->collEnt.entries[idxA].key;
                cont.normal = Vector3Scale(cont.normal, -1);
                cont.pointA = locB;
                cont.pointB = locA;
                entB->coll->contacts[entB->coll->nContacts++] = cont;
            } else {
                /*logMsg(
                    LOG_LVL_WARN, "too many collisions for collider %u",
                    sys->sysEntities.entries[j].key
                );*/
            }
        }
    }
}

void physics_raycast(PhysicsSystem *sys, Ray ray, float maxDist,
                     ColliderRayContact *cont, size_t *nCont, size_t maxCont,
                     uint32_t mask) {
    const ColliderEntity *ent;
    BoundingBox bb;
    RayCollision collRes;
    ColliderRayContact currContact;
    Mesh tempMesh;

    *nCont = 0;
    for (size_t i = 0; i < sys->collEnt.nEntries && *nCont < maxCont; i++) {
        ent = (const ColliderEntity *)sys->collEnt.entries[i].val.ptr;
        if ((mask & ent->coll->collMask) != mask)
            continue;

        bb = ent->coll->_boundsTransformed;
        collRes = GetRayCollisionBox(ray, bb);
        if (collRes.hit && collRes.distance < maxDist) {
            switch (ent->coll->type) {
            case COLLIDER_TYPE_AABB:
                break;
            case COLLIDER_TYPE_CONVEX_HULL:
                tempMesh.indices = ent->coll->convexHull.indices;
                tempMesh.triangleCount = ent->coll->convexHull.nVertices - 2;
                tempMesh.vertices = ent->coll->convexHull.vertices;
                collRes = GetRayCollisionMesh(ray, tempMesh, *ent->transform);
                break;
            default:
                logMsg(LOG_LVL_ERR, "only AABB and convex hull supported");
                collRes.hit = 0;
                break;
            }
            if (collRes.hit) {
                currContact.id = sys->collEnt.entries[i].key;
                currContact.normal = collRes.normal;
                currContact.dist = collRes.distance;
                cont[(*nCont)++] = currContact;
            }
        }
    }
}

void physics_updateCollisions(PhysicsSystem *sys) {
    physics_collisionBroadPhase(sys);
    physics_collisionNarrowPhase(sys);
}

void physics_updateBodies(PhysicsSystem *sys, float dt) {
    RigidBody *rbA, *rbB;
    ColliderEntity *collA, *collB;

    for (size_t i = 0; i < sys->nContacts; i++) {
        rbA = sys->rigidBodies.entries[sys->contacts[i].posA].val.ptr;
        rbB = sys->rigidBodies.entries[sys->contacts[i].posB].val.ptr;
        collA = sys->collEnt.entries[sys->contacts[i].posA].val.ptr;
        collB = sys->collEnt.entries[sys->contacts[i].posB].val.ptr;

        Vector3 posA = Vector3Add(
            rbA->pos, getMatrixTranslation(&collA->coll->localTransform));
        Vector3 posB = Vector3Add(
            rbB->pos, getMatrixTranslation(&collB->coll->localTransform));
        // posA = rbA->pos;
        // posB = rbB->pos;

        if (rbA->mass == .0f && rbB->mass == .0f)
            continue;

        if (!rbA->enableDynamics || !rbB->enableDynamics)
            continue;

        // Vector3 velAFull = Vector3Add(rbA->vel, rbA->angularVel);
        // Vector3 velBFull = Vector3Add(rbB->vel, rbB->angularVel);
        Vector3 velAFull = rbA->vel;
        Vector3 velBFull = rbB->vel;

        // Vector3 rv = Vector3Subtract(rbB->vel, rbA->vel);
        Vector3 rv = Vector3Subtract(velBFull, velAFull);
        Vector3 tangent = Vector3Subtract(
            rv, Vector3Scale(sys->contacts[i].normal,
                             Vector3DotProduct(sys->contacts[i].normal, rv)));
        tangent = Vector3Normalize(tangent);
        float velAlongNormal = Vector3DotProduct(rv, sys->contacts[i].normal);

        if (velAlongNormal > 0)
            continue; // velAlongNormal *= -1.f;

        float massAInv = rbA->mass ? 1.f / rbA->mass : 0;
        float massBInv = rbB->mass ? 1.f / rbB->mass : 0;

        float e = rbA->bounce < rbB->bounce ? rbA->bounce : rbB->bounce;
        float jt = -Vector3DotProduct(rv, tangent) / (massAInv + massBInv);
        float mu = sqrtf(powf(rbA->staticFriction, 2.f) +
                         powf(rbB->staticFriction, 2.f));

        Vector3 contA = sys->contacts[i].pointA;
        Vector3 contB = sys->contacts[i].pointB;
        Vector3 relPosA = Vector3Subtract(contA, posA);
        Vector3 relPosB = Vector3Subtract(contB, posB);

        Vector3 inertiaA = Vector3CrossProduct(
            Vector3Transform(
                Vector3CrossProduct(relPosA, sys->contacts[i].normal),
                rbA->inverseInertiaTensor),
            relPosA);
        Vector3 inertiaB = Vector3CrossProduct(
            Vector3Transform(
                Vector3CrossProduct(relPosB, sys->contacts[i].normal),
                rbB->inverseInertiaTensor),
            relPosB);
        float angEffect = Vector3DotProduct(Vector3Add(inertiaA, inertiaB),
                                            sys->contacts[i].normal);

        // Impulse response
        float j = -(1 + e) * velAlongNormal / (massAInv + massBInv + angEffect);
        Vector3 imp = Vector3Scale(sys->contacts[i].normal, j);

        const float percent = sys->correction.penetrationScale;
        const float kSlop = sys->correction.penetrationOffset;
        float corrDepth = sys->contacts[i].depth + kSlop;
        if (corrDepth < 0)
            corrDepth = 0;
        Vector3 corr =
            Vector3Scale(sys->contacts[i].normal,
                         (corrDepth * percent) / (massAInv + massBInv));

        if (massAInv != .0f &&
            rbA->_idleVelTicks < sys->correction.idleVelTicks) {
            // rbA->vel = Vector3Subtract(rbA->vel, Vector3Scale(imp,
            // massAInv));
            rbA->pos = Vector3Subtract(rbA->pos, Vector3Scale(corr, massAInv));
        }
        if (massBInv != .0f &&
            rbB->_idleVelTicks < sys->correction.idleVelTicks) {
            // rbB->vel = Vector3Add(rbB->vel, Vector3Scale(imp, massBInv));
            rbB->pos = Vector3Add(rbB->pos, Vector3Scale(corr, massBInv));
        }

        // Friction response
        Vector3 fImpulse = (Vector3){0, 0, 0};
        if (fabsf(jt) < j * mu)
            fImpulse = Vector3Scale(tangent, jt);
        else {
            float dynF = sqrtf(powf(rbA->dynamicFriction, 2.f) +
                               powf(rbB->dynamicFriction, 2.f));
            fImpulse = Vector3Scale(tangent, -j * dynF);
        }

        physics_applyImpulseAt(rbA, Vector3Scale(fImpulse, -1.f), relPosA);
        physics_applyImpulseAt(rbB, Vector3Scale(fImpulse, 1.f), relPosB);
        // physics_applyImpulse(rbA, Vector3Scale(fImpulse, -1.f));
        // physics_applyImpulse(rbB, Vector3Scale(fImpulse, 1.f));

        imp = Vector3SetLength(imp, Vector3Length(imp) + 0.0f);
        physics_applyImpulseAt(rbA, Vector3Scale(imp, -1.f), relPosA);
        physics_applyImpulseAt(rbB, Vector3Scale(imp, 1.f), relPosB);
    }

    for (size_t i = 0; i < sys->rigidBodies.nEntries; i++) {
        rbA = (RigidBody *)sys->rigidBodies.entries[i].val.ptr;
        physics_bodyUpdate(sys, rbA, dt);
    }
}
