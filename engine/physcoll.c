#include "./physcoll.h"


PhysicsRigidBody physics_initRigidBody(float mass) {
    PhysicsRigidBody res;
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
    res.dynamicFriction = 0.4f;
    res.mediumFriction = 0.001f;

    res.enableRot = (Vector3){1, 1, 1};
    return res;
}

PhysicsSystem physics_initSystem() {
    PhysicsSystem sys;
    sys.sysEntities = hashmap_init();
    sys.nContacts = 0;
    sys.nContactsBroad = 0;
    sys.correction.penetrationOffset = 0.1;
    sys.correction.penetrationScale = 0.6;
    return sys;
}

void physics_addRigidBody(
    PhysicsSystem *sys, uint32_t id, PhysicsRigidBody *rb,
    Collider *coll, Matrix *transform
) {
    PhysicsSystemEntity *ent = malloc(sizeof(*ent));
    ent->body = rb;
    ent->collider = coll;
    ent->transform = transform;
    ent->transformInverse = MatrixIdentity();
    hashmap_set(&sys->sysEntities, id, (HashmapVal)(void*)ent);

    logMsg(LOG_LVL_DEBUG, "added entry id %u to physics system", id);
}

void physics_removeRigidBody(PhysicsSystem *sys, uint32_t id) {
    PhysicsSystemEntity *ent;
    if(!hashmap_getP(&sys->sysEntities, id, (void**)&ent)) {
        logMsg(LOG_LVL_ERR, "entry id %u not found in physics system", id);
        return;
    }
    free(ent);
    hashmap_del(&sys->sysEntities, id, 0);
    logMsg(LOG_LVL_DEBUG, "removed entry id %u from physics system", id);
}

void physics_setPosition(PhysicsRigidBody *rb, Vector3 pos) {
    rb->pos = pos;
}

void physics_applyAngularImpulse(PhysicsRigidBody *rb, Vector3 force) {
    rb->angularVel = Vector3Add(
        rb->angularVel, Vector3Transform(force, rb->inverseInertiaTensor)
    );
}


static void physics_bodyUpdate(PhysicsRigidBody *body, float dt) {
    Vector3 totalAccel = body->accel;

    if(body->mass != .0f)
        totalAccel = Vector3Add(totalAccel, body->gravity);

    const float waterLevel = -5.f + sin(GetTime()) + 1;
    if(body->pos.y < waterLevel) {
        float depth = waterLevel - body->pos.y + 1 + 3;
        totalAccel = Vector3Add(
            totalAccel, (Vector3){0, depth * 3, 0}
        );
        body->vel = Vector3Scale(body->vel, 0.98);
    }

    body->vel = Vector3Add(body->vel, Vector3Scale(totalAccel, dt));
    body->vel = Vector3Scale(body->vel, 1.f - body->mediumFriction * dt);
    body->pos = Vector3Add(body->pos, Vector3Scale(body->vel, dt));

    // Update inverse inertia tensor
    Vector3 invIn = (Vector3){0, 0, 0};
    if(body->mass != .0f)
        invIn = Vector3Scale(body->inverseInertia, 1.f / body->mass);

    Quaternion rotQConj = (Quaternion){-body->rot.x, -body->rot.y, -body->rot.z, body->rot.w};
    Matrix invRot = QuaternionToMatrix(rotQConj);
    Matrix rot = QuaternionToMatrix(body->rot);


    body->inverseInertiaTensor = MatrixMultiply(
        rot, MatrixScale(invIn.x, invIn.y, invIn.z)
    );
    body->inverseInertiaTensor = MatrixMultiply(
        body->inverseInertiaTensor,
        invRot
    );

    // Update orientation
    Quaternion angVelQ = (Quaternion) {
        body->angularVel.x * dt * 0.5f * body->enableRot.x,
        body->angularVel.y * dt * 0.5f * body->enableRot.y,
        body->angularVel.z * dt * 0.5f * body->enableRot.z,
        0.f
    };
    body->rot = QuaternionAdd(body->rot, QuaternionMultiply(angVelQ, body->rot));
    body->rot = QuaternionNormalize(body->rot);

    // Angular velocity damping
    body->angularVel = Vector3Scale(body->angularVel, 1.f - 0.6f * dt);
}

static void physics_applyForce(PhysicsRigidBody *body, Vector3 force) {
    if(body->mass == .0f)
        return;
    body->accel = Vector3Scale(force, 1.f / body->mass);
}


static Vector3 getMatrixTranslation(Matrix *mat) {
    return (Vector3){mat->m12, mat->m13, mat->m14};
}

static void physics_collisionBroadPhase(PhysicsSystem *sys) {
    const PhysicsSystemEntity *entA, *entB;
    BoundingBox bbA, bbB;
    
    sys->nContactsBroad = 0;
    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        entA = (const PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;

        for(size_t j = i + 1; j < sys->sysEntities.nEntries; j++) {
            entB = (const PhysicsSystemEntity*)sys->sysEntities.entries[j].val.ptr;
            bbA = BoxTransform(entA->collider->bounds, *entA->transform);
            bbB = BoxTransform(entB->collider->bounds, *entB->transform);
            if(BoxIntersect(bbA, bbB)) {
                sys->contactsBroad[sys->nContactsBroad].bodyIdxA = i;
                sys->contactsBroad[sys->nContactsBroad].bodyIdxB = j;
                sys->nContactsBroad++;
            }
        }
    }
}

static GJKColliderMesh genGJKMesh(PhysicsSystemEntity *ent) {
    GJKColliderMesh gjkMesh;
    gjkMesh.vertices = ent->collider->convexHull.vertices;
    gjkMesh.nVertices = ent->collider->convexHull.nVertices;
    gjkMesh.pos = getMatrixTranslation(ent->transform);
    gjkMesh.transform = *ent->transform;
    gjkMesh.transform.m12 = 0;
    gjkMesh.transform.m13 = 0;
    gjkMesh.transform.m14 = 0;
    gjkMesh.transformInverse = MatrixInvert(gjkMesh.transform);
    return gjkMesh;
}

static void physics_collisionNarrowPhase(PhysicsSystem *sys) {
    PhysicsSystemEntity *entA, *entB;
    Collider *collA, *collB;
    GJKColliderMesh gjkMeshA, gjkMeshB;
    ColliderContact cont;
    int idxA, idxB;

    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        entA = (PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;
        collA = entA->collider;
        collA->nContacts = 0;
        entA->transformInverse = MatrixInvert(*entA->transform);
    }
    
    sys->nContacts = 0;
    for(size_t i = 0; i < sys->nContactsBroad; i++) {
        idxA = sys->contactsBroad[i].bodyIdxA;
        idxB = sys->contactsBroad[i].bodyIdxB;
        entA = sys->sysEntities.entries[idxA].val.ptr;
        entB = sys->sysEntities.entries[idxB].val.ptr;
        collA = entA->collider;
        collB = entB->collider;

        gjkMeshA = genGJKMesh(entA);
        gjkMeshB = genGJKMesh(entB);

        if((collA->collTargetMask & collB->collMask) != collA->collTargetMask)
            continue;
        
        if(collA->type != COLLIDER_TYPE_CONVEX_HULL || collB->type != COLLIDER_TYPE_CONVEX_HULL) {
            logMsg(LOG_LVL_WARN, "only convex hull collision is supported");
            continue;
        }
        
        Vector3 nor, locA, locB;
        uint8_t collRes = gjk(&gjkMeshA, &gjkMeshB, &nor, &locA, &locB);
        if(collRes) {
            if(isnan(nor.x) || isnan(nor.y) || isnan(nor.z)) {
                printf("nor: %g %g %g\n", nor.x, nor.y, nor.z);
                continue;
            }
            if(isnan(locA.x) || isnan(locA.y) || isnan(locA.z)) {
                printf("locA: %g %g %g\n", locA.x, locA.y, locA.z);
                continue;
            }
            if(isnan(locB.x) || isnan(locB.y) || isnan(locB.z)) {
                printf("locB: %g %g %g\n", locB.x, locB.y, locB.z);
                continue;
            }

            cont.depth = Vector3Length(nor);
            cont.normal = Vector3Normalize(nor);

            if(sys->nContacts < PHYSICS_WORLD_MAX_CONTACTS) {
                sys->contacts[sys->nContacts].bodyA = entA->body;
                sys->contacts[sys->nContacts].bodyB = entB->body;
                sys->contacts[sys->nContacts].depth = cont.depth;
                sys->contacts[sys->nContacts].normal = cont.normal;
                sys->contacts[sys->nContacts].pointA = locA;
                sys->contacts[sys->nContacts].pointB = locB;
                sys->nContacts++;
            }
            else logMsg(LOG_LVL_ERR, "too many collisions in system");

            if(collA->nContacts < COLLIDER_MAX_CONTACTS) {
                cont.sourceId = sys->sysEntities.entries[idxA].key;
                cont.targetId = sys->sysEntities.entries[idxB].key;
                cont.pointA = locA;
                cont.pointB = locB;
                collA->contacts[collA->nContacts++] = cont;
            }
            else {
                /*logMsg(
                    LOG_LVL_WARN, "too many collisions for collider %u",
                    sys->sysEntities.entries[i].key
                );*/
            }
            if(collB->nContacts < COLLIDER_MAX_CONTACTS) {
                cont.sourceId = sys->sysEntities.entries[idxB].key;
                cont.targetId = sys->sysEntities.entries[idxA].key;
                cont.normal = Vector3Scale(cont.normal, -1);
                cont.pointA = locB;
                cont.pointB = locA;
                collB->contacts[collB->nContacts++] = cont;
            }
            else {
                /*logMsg(
                    LOG_LVL_WARN, "too many collisions for collider %u",
                    sys->sysEntities.entries[j].key
                );*/
            }
        }
    }
}

void physics_updateCollisions(PhysicsSystem *sys) {
    physics_collisionBroadPhase(sys);
    physics_collisionNarrowPhase(sys);
}

void physics_updateSystem(PhysicsSystem *sys, float dt) {
    PhysicsRigidBody *rbA, *rbB;
    PhysicsSystemEntity *ent;
    physics_updateCollisions(sys);

    //logMsg(LOG_LVL_WARN, "%u contacts", sys->nContacts);

    for(size_t i = 0; i < sys->nContacts; i++) {
        rbA = sys->contacts[i].bodyA;
        rbB = sys->contacts[i].bodyB;

        if(rbA->mass == .0f && rbB->mass == .0f)
            continue;

        Vector3 velAFull = rbA->vel;//Vector3Add(rbA->vel, rbA->angularVel);
        Vector3 velBFull = rbB->vel;//Vector3Add(rbB->vel, rbB->angularVel);

        //Vector3 rv = Vector3Subtract(rbB->vel, rbA->vel);
        Vector3 rv = Vector3Subtract(velBFull, velAFull);
        Vector3 tangent = Vector3Subtract(rv, Vector3Scale(
            sys->contacts[i].normal,
            Vector3DotProduct(sys->contacts[i].normal, rv))
        );
        tangent = Vector3Normalize(tangent);
        float velAlongNormal = Vector3DotProduct(rv, sys->contacts[i].normal);

        if(velAlongNormal > 0)
            continue; //velAlongNormal *= -1.f;
        
        float massAInv = rbA->mass ? 1.f / rbA->mass : 0;
        float massBInv = rbB->mass ? 1.f / rbB->mass : 0;


        float e = rbA->bounce < rbB->bounce ? rbA->bounce : rbB->bounce;
        float jt = -Vector3DotProduct(rv, tangent) / (massAInv + massBInv);
        float mu = sqrtf(powf(rbA->staticFriction, 2.f) + powf(rbB->staticFriction, 2.f));

        Vector3 relativeA = Vector3Subtract(sys->contacts[i].pointA, rbA->pos);
        Vector3 relativeB = Vector3Subtract(sys->contacts[i].pointB, rbB->pos);

        /*Vector3 inertiaA = Vector3CrossProduct(
            Vector3Transform(
                Vector3CrossProduct(relativeA, sys->contacts[i].normal),
                rbA->inverseInertiaTensor
            ),
            relativeA
        );
        Vector3 inertiaB = Vector3CrossProduct(
            Vector3Transform(
                Vector3CrossProduct(relativeB, sys->contacts[i].normal),
                rbB->inverseInertiaTensor
            ),
            relativeB
        );
        float angularEffect = Vector3DotProduct(
            Vector3Add(inertiaA, inertiaB), sys->contacts[i].normal
        );
        */

        // Impulse response
        float j = -(1 + e) * velAlongNormal / (massAInv + massBInv /*+ angularEffect*/);
        Vector3 imp = Vector3Scale(sys->contacts[i].normal, j);


        const float percent = sys->correction.penetrationScale;
        const float kSlop = sys->correction.penetrationOffset;
        float corrDepth = sys->contacts[i].depth - kSlop;
        if(corrDepth < 0) corrDepth = 0;
        Vector3 corr = Vector3Scale(
            sys->contacts[i].normal,
            corrDepth / (massAInv + massBInv) * percent
        );

        if(massAInv != .0f) {
            rbA->vel = Vector3Subtract(rbA->vel, Vector3Scale(imp, massAInv));
            rbA->pos = Vector3Subtract(rbA->pos, Vector3Scale(corr, massAInv));
        }
        if(massBInv != .0f) {
            rbB->vel = Vector3Add(rbB->vel, Vector3Scale(imp, massBInv));
            rbB->pos = Vector3Add(rbB->pos, Vector3Scale(corr, massBInv));
        }

        // Friction response
        Vector3 fImpulse = (Vector3){0, 0, 0};
        if(fabsf(jt) < j * mu)
            fImpulse = Vector3Scale(tangent, jt);
        else {
            float dynF = sqrtf(
                powf(rbA->dynamicFriction, 2.f) + powf(rbB->dynamicFriction, 2.f)
            );
            fImpulse = Vector3Scale(tangent, -j * dynF);
        }
        if(massAInv != .0f)
            rbA->vel = Vector3Subtract(rbA->vel, Vector3Scale(fImpulse, massAInv));
        if(massBInv != .0f)
            rbB->vel = Vector3Add(rbB->vel, Vector3Scale(fImpulse, massBInv));

        Vector3 angularImp;
        if(sys->contacts[i].depth > 0.05f) {
            if(massAInv != .0f) {
                angularImp = Vector3CrossProduct(
                    Vector3Subtract(sys->contacts[i].pointA, rbA->pos),
                    Vector3Negate(imp)
                );
                angularImp = Vector3Scale(angularImp, dt);
                physics_applyAngularImpulse(rbA, angularImp);
            }
            if(massBInv != .0f) {
                angularImp = Vector3CrossProduct(
                    Vector3Subtract(sys->contacts[i].pointB, rbB->pos),
                    imp
                );
                angularImp = Vector3Scale(angularImp, dt);
                physics_applyAngularImpulse(rbB, angularImp);
            }
        }
    }

    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        ent = (PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;
        rbA = ent->body;

        physics_bodyUpdate(rbA, dt);
    }
}
