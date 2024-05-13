#include "physics.h"


PhysicsRigidBody physics_initRigidBody(float mass) {
    PhysicsRigidBody res;
    res.accel = (Vector3){0, 0, 0};
    res.pos = (Vector3){0, 0, 0};
    res.oldPos = (Vector3){0, 0, 0};
    res.mass = mass;
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

    ent->body->oldPos = ent->body->pos;
    logMsg(LOG_LVL_WARN, "rb pos=%f %f %f", ent->body->pos.x, ent->body->pos.y, ent->body->pos.z);
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
    rb->oldPos = pos;
}


void physics_bodyUpdate(PhysicsRigidBody *body, float dt) {
    Vector3 disp = Vector3Subtract(body->pos, body->oldPos);
    body->oldPos = body->pos;
    body->pos = Vector3Add(body->pos, disp);
    body->pos = Vector3Add(body->pos, Vector3Scale(body->accel, dt * dt));
    body->accel = (Vector3){0, 0, 0};
}

static void physics_applyForce(PhysicsRigidBody *body, Vector3 force) {
    if(body->mass == .0f)
        return;
    body->accel = Vector3Add(body->accel, Vector3Scale(force, 1.f / body->mass));
}


static Vector3 getMatrixTranslation(Matrix *mat) {
    return (Vector3){mat->m12, mat->m13, mat->m14};
}

void physics_updateSystem(PhysicsSystem *sys, float dt) {
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
    
    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        entA = (PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;
        collA = entA->collider;
        gjkMeshA.vertices = collA->convexHull.vertices;
        gjkMeshA.nVertices = collA->convexHull.nVertices;
        gjkMeshA.pos = getMatrixTranslation(entA->transform);
        gjkMeshA.transform = *entA->transform;
        gjkMeshA.transformInverse = entA->transformInverse;
        for(size_t j = i + 1; j < sys->sysEntities.nEntries; j++) {
            entB = (PhysicsSystemEntity*)sys->sysEntities.entries[j].val.ptr;
            collB = entB->collider;
            gjkMeshB.vertices = collB->convexHull.vertices;
            gjkMeshB.nVertices = collB->convexHull.nVertices;
            gjkMeshB.pos = getMatrixTranslation(entB->transform);
            gjkMeshB.transform = *entB->transform;
            gjkMeshB.transformInverse = entB->transformInverse;
            
            //logMsg(LOG_LVL_DEBUG, "check coll against %u and %u", i, j);

            if((collA->collTargetMask & collB->collMask) != collA->collTargetMask)
                continue;
            
            if(collA->type != COLLIDER_TYPE_CONVEX_HULL || collB->type != COLLIDER_TYPE_CONVEX_HULL) {
                logMsg(LOG_LVL_WARN, "only convex hull collision is supported");
                continue;
            }
            //uint8_t collRes = computeGJK(collA->convexHull, collB->convexHull, matA, matB);
            
            uint8_t collRes = gjk(&gjkMeshA, &gjkMeshB, &cont.normal);
            if(collRes) {
                cont.depth = 0;
                //cont.normal = (Vector3){0, 0, 0};

                //logMsg(LOG_LVL_INFO, "Collide between %u and %u", i, j);

                if(collA->nContacts < PHYSICS_MAX_CONTACTS) {
                    cont.targetId = sys->sysEntities.entries[j].key;
                    collA->contacts[collA->nContacts++] = cont;
                    //printf("[%.2f %.2f %.2f]\n", cont.normal.x, cont.normal.y, cont.normal.z);
                }
                else {
                    logMsg(
                        LOG_LVL_WARN, "too many collisions for collider %u",
                        sys->sysEntities.entries[i].key
                    );
                }
                if(collB->nContacts < PHYSICS_MAX_CONTACTS) {
                    cont.targetId = sys->sysEntities.entries[i].key;
                    cont.normal = Vector3Scale(cont.normal, -1);
                    collB->contacts[collB->nContacts++] = cont;
                }
                else {
                    logMsg(
                        LOG_LVL_WARN, "too many collisions for collider %u",
                        sys->sysEntities.entries[j].key
                    );
                }
            }
        }
    }

    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        entA = (PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;
        Collider *coll = entA->collider;
        PhysicsRigidBody *rb = entA->body;
        physics_applyForce(rb, (Vector3){0, -5.8 * rb->mass, 0});
        for(int j = 0; j < coll->nContacts; j++) {
            Vector3 n = Vector3Scale(coll->contacts[j].normal, 1);
            if(rb->mass != 0) {
                logMsg(LOG_LVL_WARN, "normal: %.2f %.2f %.2f", n.x, n.y, n.z);
                physics_setPosition(rb, Vector3Subtract(rb->pos, n));
                //rb->pos = Vector3Subtract(rb->pos, n);
                //rb->pos = n;
            }
        }
        //printf("[%.2f %.2f %.2f]\n", rb->pos.x, rb->pos.y, rb->pos.z);
        physics_bodyUpdate(rb, dt);
        //*entA->transform = MatrixTranslate(rb->pos.x, rb->pos.y, rb->pos.z);
    }
}
