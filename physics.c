#include "physics.h"


PhysicsRigidBody physics_initRigidBody(float mass) {
    PhysicsRigidBody res;
    res.vel = (Vector3){0, 0, 0};
    res.accel = (Vector3){0, 0, 0};
    res.pos = (Vector3){0, 0, 0};
    res.gravity = (Vector3){0, -9.8f, 0};

    res.angularVel = (Vector3){0, 0, 0};
    //res.torque = (Vector3){0, 0, 0};
    res.inverseInertia = (Vector3){.05f, .05f, .05f};
    res.inverseInertiaTensor = MatrixIdentity();
    res.rot = QuaternionIdentity();

    res.mass = mass;
    res.bounce = .0f;
    res.staticFriction = 0.1f;
    res.dynamicFriction = 0.4f;
    res.mediumFriction = 0.001f;

    res.enableAngularVel = 1;
    return res;
}

PhysicsSystem physics_initSystem() {
    PhysicsSystem sys;
    sys.sysEntities = hashmap_init();
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

    //ent->body->oldPos = ent->body->pos;
    //logMsg(LOG_LVL_WARN, "rb pos=%f %f %f", ent->body->pos.x, ent->body->pos.y, ent->body->pos.z);
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
    //rb->angularVel = Vector3Add(rb->angularVel, Vector3Scale(force, 1.f / 12));
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

    if(body->enableAngularVel) {
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
            body->angularVel.x * dt * 0.5f,
            body->angularVel.y * dt * 0.5f,
            body->angularVel.z * dt * 0.5f,
            0.f
        };
        body->rot = QuaternionAdd(body->rot, QuaternionMultiply(angVelQ, body->rot));
        body->rot = QuaternionNormalize(body->rot);

        // Angular velocity damping
        body->angularVel = Vector3Scale(body->angularVel, 1.f - 5.8f * dt);
    }
}

static void physics_applyForce(PhysicsRigidBody *body, Vector3 force) {
    if(body->mass == .0f)
        return;
    body->accel = Vector3Scale(force, 1.f / body->mass);
}


static Vector3 getMatrixTranslation(Matrix *mat) {
    return (Vector3){mat->m12, mat->m13, mat->m14};
}

static void physics_updateCollisions(PhysicsSystem *sys) {
    PhysicsSystemEntity *entA, *entB;
    Collider *collA, *collB;
    GJKColliderMesh gjkMeshA, gjkMeshB;
    ColliderContact cont;

    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        entA = (PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;
        collA = entA->collider;
        collA->nContacts = 0;
        entA->transformInverse = MatrixInvert(*entA->transform);
    }
    
    sys->nContacts = 0;
    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        entA = (PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;
        collA = entA->collider;
        gjkMeshA.vertices = collA->convexHull.vertices;
        gjkMeshA.nVertices = collA->convexHull.nVertices;
        gjkMeshA.pos = getMatrixTranslation(entA->transform);
        gjkMeshA.transform = *entA->transform;
        gjkMeshA.transform.m12 = 0;
        gjkMeshA.transform.m13 = 0;
        gjkMeshA.transform.m14 = 0;
        //gjkMeshA.transformInverse = entA->transformInverse;
        gjkMeshA.transformInverse = MatrixInvert(gjkMeshA.transform);
        for(size_t j = i + 1; j < sys->sysEntities.nEntries; j++) {
            entB = (PhysicsSystemEntity*)sys->sysEntities.entries[j].val.ptr;
            collB = entB->collider;
            gjkMeshB.vertices = collB->convexHull.vertices;
            gjkMeshB.nVertices = collB->convexHull.nVertices;
            gjkMeshB.pos = getMatrixTranslation(entB->transform);
            gjkMeshB.transform = *entB->transform;
            gjkMeshB.transform.m12 = 0;
            gjkMeshB.transform.m13 = 0;
            gjkMeshB.transform.m14 = 0;
            //gjkMeshB.transformInverse = entB->transformInverse;
            gjkMeshB.transformInverse = MatrixInvert(gjkMeshB.transform);
            
            //logMsg(LOG_LVL_DEBUG, "check coll against %u and %u", i, j);

            if((collA->collTargetMask & collB->collMask) != collA->collTargetMask)
                continue;
            
            if(collA->type != COLLIDER_TYPE_CONVEX_HULL || collB->type != COLLIDER_TYPE_CONVEX_HULL) {
                logMsg(LOG_LVL_WARN, "only convex hull collision is supported");
                continue;
            }
            //uint8_t collRes = computeGJK(collA->convexHull, collB->convexHull, matA, matB);
            
            Vector3 nor, locA, locB;
            uint8_t collRes = gjk(&gjkMeshA, &gjkMeshB, &nor, &locA, &locB);
            if(collRes) {
                cont.pointA = locA;
                cont.pointB = locB;
                cont.depth = Vector3Length(nor);
                cont.normal = Vector3Normalize(nor);
                //cont.normal = (Vector3){0, 0, 0};

                //logMsg(LOG_LVL_INFO, "Collide between %u and %u", i, j);

                if(sys->nContacts < PHYSICS_WORLD_MAX_CONTACTS) {
                    sys->contacts[sys->nContacts].bodyA = entA->body;
                    sys->contacts[sys->nContacts].bodyB = entB->body;
                    sys->contacts[sys->nContacts].depth = cont.depth;
                    sys->contacts[sys->nContacts].normal = cont.normal;
                    sys->contacts[sys->nContacts].pointA = cont.pointA;
                    sys->contacts[sys->nContacts].pointB = cont.pointB;
                    sys->nContacts++;
                }
                else logMsg(LOG_LVL_ERR, "too many collisions in system");

                if(collA->nContacts < COLLIDER_MAX_CONTACTS) {
                    cont.targetId = sys->sysEntities.entries[j].key;
                    collA->contacts[collA->nContacts++] = cont;
                    //printf("[%.2f %.2f %.2f]\n", cont.normal.x, cont.normal.y, cont.normal.z);
                }
                else {
                    /*logMsg(
                        LOG_LVL_WARN, "too many collisions for collider %u",
                        sys->sysEntities.entries[i].key
                    );*/
                }
                if(collB->nContacts < COLLIDER_MAX_CONTACTS) {
                    cont.targetId = sys->sysEntities.entries[i].key;
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

        Vector3 inertiaA = Vector3CrossProduct(
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

        angularEffect *= 0;

        // Impulse response
        float j = -(1 + e) * velAlongNormal / (massAInv + massBInv + angularEffect);
        Vector3 imp = Vector3Scale(sys->contacts[i].normal, j);


        const float percent = 0.6;
        const float kSlop = 0.1;
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
                physics_applyAngularImpulse(rbA, angularImp);
            }
            if(massBInv != .0f) {
                angularImp = Vector3CrossProduct(
                    Vector3Subtract(sys->contacts[i].pointB, rbB->pos),
                    imp
                );
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
