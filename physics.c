#include "physics.h"


PhysicsRigidBody physics_initRigidBody(float mass) {
    PhysicsRigidBody res;
    res.accel = (Vector3){0, 0, 0};
    res.pos = (Vector3){0, 0, 0};
    res.oldPos = (Vector3){0, 0, 0};
    res.mass = mass;
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
    ent->body = rb;;
    ent->collider = coll;
    ent->transform = transform;
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


void physics_bodyUpdate(PhysicsRigidBody *body, float dt) {
    Vector3 disp = Vector3Subtract(body->pos, body->oldPos);
    body->oldPos = body->pos;
    body->pos = Vector3Add(body->pos, disp);
    body->pos = Vector3Add(body->pos, Vector3Scale(body->accel, dt * dt));
    body->accel = (Vector3){0, 0, 0};
}


static Vector3 findFurthestPoint(ColliderMesh mesh, Vector3 dir, Matrix transform) {
    Vector3 vert, maxPoint;
    float maxDist = -100000.f, dist;
    for(int i = 0; i < mesh.nVertices * 3; i += 3) {
        vert = (Vector3){mesh.vertices[i], mesh.vertices[i + 1], mesh.vertices[i + 2]};
        vert = Vector3Transform(vert, transform);
        dist = Vector3DotProduct(vert, dir);
        if(dist > maxDist)
            maxDist = dist, maxPoint = vert;
    }
    return maxPoint;
}

static Vector3 findSupport(ColliderMesh meshA, ColliderMesh meshB, Vector3 dir, Matrix transformA, Matrix transformB) {
    Vector3 res = Vector3Subtract(
        findFurthestPoint(meshA, dir, transformA),
        findFurthestPoint(meshB, Vector3Negate(dir), transformB)
    );
    return res;
}

static void simplexPushFront(Vector3 *simplex, Vector3 point) {
    simplex[3] = simplex[2];
    simplex[2] = simplex[1];
    simplex[1] = simplex[0];
    simplex[0] = point;
}


static uint8_t sameDirection(Vector3 a, Vector3 b) {
    return Vector3DotProduct(a, b) >= 0;
}

static uint8_t nextSimplexLine(Vector3 *points, int *nPoints, Vector3 *direction) {
    const Vector3 ab = Vector3Subtract(points[1], points[0]);
    const Vector3 ao = Vector3Negate(points[0]);
    if(sameDirection(ab, ao)) {
        *direction = Vector3CrossProduct(ab, ao);
        *direction = Vector3CrossProduct(*direction, ab);
    }
    else {
        *nPoints = 1;
        *direction = ao;
    }
    return 0;
}

static uint8_t nextSimplexTriangle(Vector3 *points, int *nPoints, Vector3 *direction) {
    const Vector3 ab = Vector3Subtract(points[1], points[0]);
    const Vector3 ac = Vector3Subtract(points[2], points[0]);
    const Vector3 ao = Vector3Negate(points[0]);
    const Vector3 abc = Vector3CrossProduct(ab, ac);
    if(sameDirection(Vector3CrossProduct(abc, ac), ao)) {
        if(sameDirection(ac, ao)) {
            points[1] = points[2];
            *nPoints = 2;
            *direction = Vector3CrossProduct(ac, ao);
            *direction = Vector3CrossProduct(*direction, ac);
        }
        else {
            *nPoints = 2;
            return nextSimplexLine(points, nPoints, direction);
        }
    }
    else {
        if(sameDirection(Vector3CrossProduct(ab, abc), ao)) {
            *nPoints = 2;
            return nextSimplexLine(points, nPoints, direction);
        }
        else {
            if(sameDirection(abc, ao)) {
                *direction = abc;
            }
            else {
                Vector3 temp = points[1];
                points[1] = points[2];
                points[2] = temp;
                *direction = Vector3Negate(abc);
            }
        }
    }
    return 0;
}

static uint8_t nextSimplexTetrahedron(Vector3 *points, int *nPoints, Vector3 *direction) {
    const Vector3 ab = Vector3Subtract(points[1], points[0]);
    const Vector3 ac = Vector3Subtract(points[2], points[0]);
    const Vector3 ad = Vector3Subtract(points[3], points[0]);
    const Vector3 ao = Vector3Negate(points[0]);
    const Vector3 abc = Vector3CrossProduct(ab, ac);
    const Vector3 acd = Vector3CrossProduct(ac, ad);
    const Vector3 adb = Vector3CrossProduct(ad, ab);

    if(sameDirection(abc, ao)) {
        *nPoints = 3;
        return nextSimplexTriangle(points, nPoints, direction);
    }
    if(sameDirection(acd, ao)) {
        *nPoints = 3;
        points[1] = points[2];
        points[2] = points[3];
        return nextSimplexTriangle(points, nPoints, direction);
    }
    if(sameDirection(adb, ao)) {
        *nPoints = 3;
        points[2] = points[1];
        points[1] = points[3];
        return nextSimplexTriangle(points, nPoints, direction);
    }

    return 1;
}

static uint8_t nextSimplex(Vector3 *points, int *nPoints, Vector3 *direction) {
    switch(*nPoints) {
        case 2:
            return nextSimplexLine(points, nPoints, direction);
        case 3:
            return nextSimplexTriangle(points, nPoints, direction);
        case 4:
            return nextSimplexTetrahedron(points, nPoints, direction);
        default:
            logMsg(LOG_LVL_ERR, "invalid number of points: %u", *nPoints);
    }
    return 0;
}

uint8_t computeGJK(ColliderMesh meshA, ColliderMesh meshB, Matrix transformA, Matrix transformB) {
    /*Vector3 support = findSupport(meshA, meshB, (Vector3){1, 0, 0}, transformA, transformB);
    Vector3 points[4];
    Vector3 direction = Vector3Normalize((Vector3){-support.x, -support.y, -support.z});
    int nPoints = 1;
    points[0] = support;

    int iterations = 0;

    while((iterations++) < 1000) {
        logMsg(LOG_LVL_DEBUG, "loop");
        support = findSupport(meshA, meshB, direction, transformA, transformB);
        if(!sameDirection(support, direction))
            return 0; // no collision
        simplexPushFront(points, support);
        if(nPoints < 4)
            nPoints++;
        if(nextSimplex(points, &nPoints, &direction))
            return 1;
    }*/
    Vector3 dir = (Vector3){1, 0, 0.3};
    Vector3 support = findSupport(meshA, meshB, dir, transformA, transformB);
    int nPoints = 1;
    Vector3 points[4];
    points[0] = support;
    dir = Vector3Negate(points[0]);
    int i = 0;
    while((i++) < 64) {
        Vector3 A = findSupport(meshA, meshB, dir, transformA, transformB);
        if(!sameDirection(A, dir))
            return 0;
        simplexPushFront(points, A);
        nPoints++;
        if(nextSimplex(points, &nPoints, &dir))
            return 1;
    }
}


void physics_updateSystem(PhysicsSystem *sys, float dt) {
    PhysicsSystemEntity *entA, *entB;
    Collider *collA, *collB;

    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        entA = (PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;
        collA = entA->collider;
        collA->nContacts = 0;
    }
    
    for(size_t i = 0; i < sys->sysEntities.nEntries; i++) {
        entA = (PhysicsSystemEntity*)sys->sysEntities.entries[i].val.ptr;
        collA = entA->collider;
        for(size_t j = i + 1; j < sys->sysEntities.nEntries; j++) {
            entB = (PhysicsSystemEntity*)sys->sysEntities.entries[j].val.ptr;
            collB = entB->collider;
            
            logMsg(LOG_LVL_DEBUG, "check coll against %u and %u\n", i, j);

            if((collA->collTargetMask & collB->collMask) != collA->collTargetMask)
                continue;
            
            if(collA->type != COLLIDER_TYPE_CONVEX_HULL || collB->type != COLLIDER_TYPE_CONVEX_HULL) {
                logMsg(LOG_LVL_WARN, "only convex hull collision is supported");
                continue;
            }

            Matrix matA = MatrixIdentity();
            Matrix matB = MatrixIdentity();
            matA = *entA->transform, matB = *entB->transform;
            uint8_t collRes = computeGJK(collA->convexHull, collB->convexHull, matA, matB);

            if(collRes) {
                ColliderContact cont;
                cont.depth = 0;
                cont.normal = (Vector3){0, 0, 0};

                logMsg(LOG_LVL_INFO, "COLLIDE");

                if(collA->nContacts < PHYSICS_MAX_CONTACTS) {
                    cont.targetId = sys->sysEntities.entries[j].key;
                    collA->contacts[collA->nContacts++] = cont;
                }
                else {
                    logMsg(
                        LOG_LVL_WARN, "too many collisions for collider %u",
                        sys->sysEntities.entries[i].key
                    );
                }
                if(collB->nContacts < PHYSICS_MAX_CONTACTS) {
                    cont.targetId = sys->sysEntities.entries[i].key;
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
}
