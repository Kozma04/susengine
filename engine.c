#include "engine.h"


void engine_init(Engine *const engine) {
    engine->timescale = 1;
    engine->nPendingMsg = 0;
    engine->render.models = hashmap_init();
    engine->render.shaders = hashmap_init();
    engine->render.meshRend = array_init();
    //engine->render.meshRendVisible = array_init();
    //engine->render.meshRendVisibleDist = array_init();
    engine->render.lightSrc = array_init();
    engine->render.camera = ECS_INVALID_ID;
    engine->phys = physics_initSystem();
    ecs_init(&engine->ecs);
}

// Should be called right after the entity is fully registered in the ECS
void engine_entityPostCreate(Engine *const engine, const ECSEntityID id) {
    EngineCallbackData cbData;
    cbData.engine = engine;
    ecs_execCallbackAllComp(&engine->ecs, id, ENGINE_ECS_CB_TYPE_ON_CREATE,
                            &cbData);
}

void engine_entityDestroy(Engine *const engine, const ECSEntityID id) {
    EngineCallbackData cbData;
    cbData.engine = engine;
    ecs_execCallbackAllComp(&engine->ecs, id, ENGINE_ECS_CB_TYPE_ON_DESTROY,
                            &cbData);
    ecs_unregisterEntity(&engine->ecs, id);
}


static void engine_dispatchMessage(Engine *const engine, EngineMsg *const msg) {
    uint32_t ent;
    EngineECSCompData *compData;
    EngineCallbackData cbData;
    cbData.engine = engine;
    cbData.msgRecv.msg = msg;
    // "broadcast" message
    if(msg->dstId == ECS_INVALID_ID) {
        for(ent = 0; ent < engine->ecs.nActiveEnt; ent++) {
            ecs_getCompData(
                &engine->ecs, engine->ecs.activeEnt[ent],
                ENGINE_ECS_COMP_TYPE_INFO, (void**)&compData
            );
            if(compData == NULL) {
                logMsg(
                    LOG_LVL_FATAL, "entity %u(\"%s\") has no Info component!",
                    ent, ecs_getEntityNameCstrP(&engine->ecs, ent)
                );
                return;
            }
            if((compData->info.typeMask & msg->dstMask) == msg->dstMask) {
                ecs_execCallbackAllComp(
                    &engine->ecs, ent, ENGINE_ECS_CB_TYPE_ON_MSG_RECV, 
                    &cbData
                );
            }
        }
    }
    else {
        if(ecs_entityExists(&engine->ecs, msg->dstId) != ECS_RES_OK) {
            logMsg(LOG_LVL_WARN, "message dst. entity %u not found", msg->dstId);
            return;
        }
        ecs_execCallbackAllComp(
            &engine->ecs, msg->dstId, ENGINE_ECS_CB_TYPE_ON_MSG_RECV,
            &cbData
        );
    }
}

static void engine_dispatchMessages(Engine *const engine) {
    uint32_t i, ent;
    EngineMsg *msg;
    EngineECSCompData *compData;
    for(i = 0; i < engine->nPendingMsg; i++)
        engine_dispatchMessage(engine, engine->pendingMsg + i);
    engine->nPendingMsg = 0;
}

static void engine_execUpdateCallbacks(Engine *const engine,
                                       const float deltaTime) {
    EngineCallbackData cbData;
    cbData.engine = engine;
    cbData.update.deltaTime = deltaTime * engine->timescale;
    ecs_execCallbackAllEnt(
        &engine->ecs, ENGINE_ECS_CB_TYPE_ON_UPDATE, &cbData
    );
}

static void engine_updateTransform(Engine *const engine, 
                                   EngineECSCompData *cData) {
    EngineECSCompData *parentTrans = NULL;
    if(cData->trans.anchor != ECS_INVALID_ID) {
        ecs_getCompData(
            &engine->ecs, cData->trans.anchor,
            ENGINE_ECS_COMP_TYPE_TRANSFORM, (void**)&parentTrans
        );
        if(parentTrans->trans._globalUpdate)
            engine_updateTransform(engine, parentTrans);
    }
    EngineCompTransform *const trans = &cData->trans;
    if(trans->localUpdate) {
        trans->localMatrix = MatrixScale(
            trans->scale.x, trans->scale.y, trans->scale.z
        );
        trans->localMatrix = MatrixMultiply(
            trans->localMatrix,
            QuaternionToMatrix(trans->rot)
        );
        trans->localMatrix = MatrixMultiply(
            trans->localMatrix,
            MatrixTranslate(trans->pos.x, trans->pos.y, trans->pos.z)
        );
    }
    if(parentTrans == NULL)
        trans->globalMatrix = trans->localMatrix;
    else {
        trans->globalMatrix = MatrixMultiply(
            trans->localMatrix,
            parentTrans->trans.globalMatrix
        );
    }
    //trans->globalMatrix = MatrixIdentity();
    trans->localUpdate = 0;
    trans->_globalUpdate = 0;
}

static void engine_updateTransforms(Engine *const engine) {
    ECSEntityID entId;
    EngineECSCompData *cData;
    ECSStatus stat;
    for(uint32_t i = 0; i < engine->ecs.nActiveEnt; i++) {
        entId = engine->ecs.activeEnt[i];
        stat = ecs_getCompData(
            &engine->ecs, entId, ENGINE_ECS_COMP_TYPE_TRANSFORM, (void**)&cData
        );
        if(cData == NULL) {
            logMsg(LOG_LVL_FATAL, "can't find transform for entity %u", entId);
            return;
        }
        cData->trans._globalUpdate = 1;
    }
    for(uint32_t i = 0; i < engine->ecs.nActiveEnt; i++) {
        entId = engine->ecs.activeEnt[i];
        ecs_getCompData(
            &engine->ecs, entId, ENGINE_ECS_COMP_TYPE_TRANSFORM, (void**)&cData
        );
        if(cData->trans._globalUpdate) {
            engine_updateTransform(engine, cData);
        }
    }
}


void engine_stepUpdate(Engine *const engine, const float deltaTime) {
    const static int physSubsteps = 1;

    engine_dispatchMessages(engine);
    for(int i = 0; i < physSubsteps; i++) {
        physics_updateSystem(&engine->phys, 1.f / 60.f / physSubsteps);
    }
    engine_updateTransforms(engine);
    engine_execUpdateCallbacks(engine, deltaTime);
}


EngineStatus engine_render_addModel(
    Engine *const engine, const EngineRenderModelID id,
    const Model* const model
) {
    if(hashmap_get(&engine->render.models, id, NULL)) {
        logMsg(LOG_LVL_WARN, "model id %u already present, ignoring add", id);
        return ENGINE_STATUS_RENDER_DUPLICATE_ITEM;
    }
    hashmap_set(&engine->render.models, id, (HashmapVal)(void*)model);
    logMsg(LOG_LVL_INFO, "added model id %u", id);
    return ENGINE_STATUS_OK;
}

Model *engine_render_getModel(Engine *engine, EngineRenderModelID id) {
    Model *res;
    if(!hashmap_getP(&engine->render.models, id, (void**)&res))
        return NULL;
    return res;
}

EngineStatus engine_render_addShader(
    Engine *const engine, const EngineShaderID id,
    const Shader* const shader
) {
    if(hashmap_get(&engine->render.shaders, id, NULL)) {
        logMsg(LOG_LVL_WARN, "shader id %u already present, ignoring add", id);
        return ENGINE_STATUS_RENDER_DUPLICATE_ITEM;
    }
    hashmap_set(&engine->render.shaders, id, (HashmapVal)(void*)shader);
    logMsg(LOG_LVL_INFO, "added shader id %u", id);
    return ENGINE_STATUS_OK;
}

Shader engine_render_getShader(Engine *engine, EngineShaderID id) {
    static Shader defaultShader;
    Shader *shader;
    const uint8_t res = hashmap_getP(
        &engine->render.shaders, id, (void**)&shader
    );
    defaultShader.id = rlGetShaderIdDefault();
    defaultShader.locs = rlGetShaderLocsDefault();
    if(!res) {
        logMsg(LOG_LVL_ERR, "shader id %u not found", id);
        shader = &defaultShader;
    }
    return *shader;
}

static EngineStatus engine_render_registerMeshRenderer(
    Engine *const engine, const ECSComponentID id
) {
    if(array_has(&engine->render.meshRend, (ArrayVal)id)) {
        logMsg(LOG_LVL_ERR, "duplicate mesh renderer id %u", id);
        return ENGINE_STATUS_RENDER_DUPLICATE_ITEM;
    }
    array_pushBack(&engine->render.meshRend, (ArrayVal)id);
    logMsg(LOG_LVL_DEBUG, "registered mesh renderer id %u", id);
    return ENGINE_STATUS_OK;
}

static EngineStatus engine_render_unregisterMeshRenderer(
    Engine *const engine, const ECSComponentID id
) {
    const uint32_t pos = array_at(&engine->render.meshRend, (ArrayVal)id);
    if(!array_del(&engine->render.meshRend, pos)) {
        logMsg(LOG_LVL_ERR, "mesh renderer id %u not found", id);
        return ENGINE_STATUS_RENDER_ITEM_NOT_FOUND;
    }
    logMsg(LOG_LVL_DEBUG, "unregistered mesh renderer id %u", id);
    return ENGINE_STATUS_OK;
}

EngineStatus engine_render_registerLightSrc(
    Engine *const engine, const ECSComponentID id
) {
    if(array_has(&engine->render.lightSrc, (ArrayVal)id)) {
        logMsg(LOG_LVL_ERR, "duplicate light source id %u", id);
        return ENGINE_STATUS_RENDER_DUPLICATE_ITEM;
    }
    array_pushBack(&engine->render.lightSrc, (ArrayVal)id);
    logMsg(LOG_LVL_DEBUG, "registered light source id %u", id);
    return ENGINE_STATUS_OK;
}

EngineStatus engine_render_unregisterLightSrc(
    Engine *const engine, const ECSComponentID id
) {
    const uint32_t pos = array_at(&engine->render.lightSrc, (ArrayVal)id);
    if(!array_del(&engine->render.lightSrc, pos)) {
        logMsg(LOG_LVL_ERR, "light source id %u not found", id);
        return ENGINE_STATUS_RENDER_ITEM_NOT_FOUND;
    }
    logMsg(LOG_LVL_DEBUG, "unregistered light source id %u", id);
    return ENGINE_STATUS_OK;
}

void engine_render_setCamera(Engine *engine, const ECSComponentID id) {
    engine->render.camera = id;
}


EngineStatus engine_entitySendMsg(
    Engine *const engine, const ECSEntityID src, const ECSEntityID dst,
    const EngineMsgType msgType, const void *const msgData, const size_t msgSize
) {
    if(msgSize > ENGINE_MESSAGE_DATA_SIZE) {
        logMsg(
            LOG_LVL_ERR, "max. message data size exceeded: %u vs %u", msgSize,
            ENGINE_MESSAGE_DATA_SIZE
        );
        return ENGINE_STATUS_MSG_DATA_SIZE_EXCEEDED;
    }
    if(!ecs_entityExists(&engine->ecs, src)) {
        logMsg(LOG_LVL_ERR, "source entity id %u not registered", src);
        return ENGINE_STATUS_MSG_SRC_NOT_FOUND;
    }
    if(!ecs_entityExists(&engine->ecs, dst)) {
        logMsg(LOG_LVL_ERR, "destination entity id %u not registered", dst);
        return ENGINE_STATUS_MSG_DST_NOT_FOUND;
    }
    if(engine->nPendingMsg >= ENGINE_MAX_PENDING_MESSAGES) {
        logMsg(LOG_LVL_ERR, "max. number of pending messages reached");
        return ENGINE_STATUS_MSG_PENDING_FULL;
    }
    EngineMsg *const msg = &engine->pendingMsg[engine->nPendingMsg++];
    msg->srcId = src;
    msg->dstId = dst;
    msg->msgType = msgType;
    msg->dstMask = 0xffffffff;
    memcpy(msg->msgData, msgData, msgSize);
    logMsg(
        LOG_LVL_DEBUG,
        "added message to pending list: type %u, size %u, src id %u, dst id %u",
        msgType, msgSize, src, dst
    );
    return 1;
}

EngineStatus engine_entityBroadcastMsg(
    Engine *const engine, const ECSEntityID src, const EngineEntType dstMask,
    const EngineMsgType msgType, const void *const msgData, const size_t msgSize
) {
    if(msgSize > ENGINE_MESSAGE_DATA_SIZE) {
        logMsg(
            LOG_LVL_ERR, "max. message data size exceeded: %u vs %u", msgSize,
            ENGINE_MESSAGE_DATA_SIZE
        );
        return ENGINE_STATUS_MSG_DATA_SIZE_EXCEEDED;
    }
    if(engine->nPendingMsg >= ENGINE_MAX_PENDING_MESSAGES) {
        logMsg(LOG_LVL_ERR, "max. number of pending messages reached");
        return ENGINE_STATUS_MSG_PENDING_FULL;
    }
    EngineMsg *const msg = &engine->pendingMsg[engine->nPendingMsg++];
    msg->srcId = src;
    msg->dstId = ECS_INVALID_ID;
    msg->msgType = msgType;
    msg->dstMask = dstMask;
    memcpy(msg->msgData, msgData, msgSize);
    logMsg(
        LOG_LVL_DEBUG,
        "added global message to pending list: type %u, size %u, src id %u, dst mask 0x%08x",
        msgType, msgSize, src, dstMask
    );
    return 1;
}


// Engine callbacks for the Entity Component System
static void engine_cbMeshRendererOnCreate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    engine_render_registerMeshRenderer(cbData->engine, entId);
}

static void engine_cbMeshRendererOnDestroy(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    engine_render_unregisterMeshRenderer(cbData->engine, entId);
}

static void engine_cbRigidBodyOnCreate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    EngineCompTransform *trans = engine_getTransform(cbData->engine, entId);
    Collider *coll = engine_getCollider(cbData->engine, entId);
    PhysicsRigidBody *rb = &((EngineECSCompData*)comp->data)->rigidBody;
    Matrix *mat;

    if(trans == NULL) {
        logMsg(LOG_LVL_FATAL, "no transform found for rigidbody in ent. %u", entId);
        return;
    }
    if(coll == NULL) {
        logMsg(LOG_LVL_FATAL, "no collider found for rigidbody in ent. %u", entId);
        return;
    }

    mat = &trans->globalMatrix;
    trans->pos = rb->pos;
    trans->localUpdate = 1;
    physics_addRigidBody(&cbData->engine->phys, entId, rb, coll, mat);
}

static void engine_cbRigidBodyOnUpdate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    EngineCompTransform *trans = engine_getTransform(cbData->engine, entId);
    PhysicsRigidBody *rb = &((EngineECSCompData*)comp->data)->rigidBody;

    trans->pos = rb->pos;
    trans->localUpdate = 1;
}

static void engine_cbRigidBodyOnDestroy(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    physics_removeRigidBody(&cbData->engine->phys, entId);
}

static void engine_cbLightSourceOnCreate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    engine_render_registerLightSrc(cbData->engine, compId);
}

static void engine_cbLightSourceOnDestroy(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    engine_render_unregisterLightSrc(cbData->engine, compId);
}

static void engine_cbLightSourceOnUpdate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    const EngineCompTransform *trans = engine_getTransform(cbData->engine, entId);
    EngineCompLightSrc *light = (EngineCompLightSrc*)comp->data;
    const Matrix *mat;
    
    if(light->type == ENGINE_LIGHTSRC_POINT) {
        if(trans == NULL) {
            logMsg(
                LOG_LVL_ERR, "no trans. found for point light in ent %u, comp %u",
                entId, compId
            );
            return;
        }
        mat = &trans->globalMatrix;
        light->pos = (Vector3){mat->m12, mat->m13, mat->m14};
    }
}

static void engine_cbCameraOnUpdate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    EngineCallbackData *cbData = cbUserData;
    EngineCompTransform *trans = engine_getTransform(cbData->engine, entId);
    EngineCompCamera *cam = (EngineCompCamera*)comp->data; //engine_getCamera(cbData->engine, entId);
    Matrix *mat = &trans->globalMatrix;
    if(trans == NULL) {
        logMsg(LOG_LVL_ERR, "camera id %u's entity has no transform", compId);
        return;
    }
    
    cam->cam.position = (Vector3){mat->m12, mat->m13, mat->m14};
}

static void engine_cbTransformOnCreate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    EngineCallbackData *cbData = cbUserData;
    EngineCompTransform *trans = engine_getTransform(cbData->engine, entId);
    if(trans == NULL) {
        logMsg(LOG_LVL_FATAL, "cannot find transform in component %u", compId);
        return;
    }
    engine_updateTransform(cbData->engine, (EngineECSCompData*)comp->data);
}


EngineStatus engine_createInfo(
    Engine *const engine, const ECSEntityID ent,
    const EngineEntType entTypeMask
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_INFO;
    ECSComponent compRaw;
    EngineCompInfo *const comp = &(((EngineECSCompData*)&compRaw.data)->info);
    comp->typeMask = entTypeMask;
    return ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK;
}

EngineStatus engine_createTransform(
    Engine *const engine, const ECSEntityID ent,
    const ECSEntityID transformAnchor
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_TRANSFORM;
    ECSComponent compRaw;
    EngineCompTransform *const comp = &(((EngineECSCompData*)&compRaw.data)->trans);
    comp->anchor = transformAnchor;
    comp->globalMatrix = MatrixIdentity();
    comp->localMatrix = MatrixIdentity();
    comp->pos = (Vector3){0, 0, 0};
    comp->scale = (Vector3){1, 1, 1};
    comp->rot = QuaternionFromEuler(0, 0, 0);
    comp->_globalUpdate = 0;
    comp->localUpdate = 1;
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK) {
        ecs_setCallback(
            &engine->ecs, ent, type, ENGINE_ECS_CB_TYPE_ON_CREATE,
            engine_cbTransformOnCreate
        );
        return ENGINE_STATUS_OK;
    }
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createCamera(
    Engine *const engine, const ECSEntityID ent,
    const float fov, const int projection
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_CAMERA;
    ECSComponent compRaw;
    EngineCompCamera *const comp = &(((EngineECSCompData*)&compRaw.data)->cam);
    comp->cam.fovy = fov;
    comp->cam.position = (Vector3){0, 0, 0};
    comp->cam.projection = projection;
    comp->cam.target = (Vector3){0, 0, 1};
    comp->cam.up = (Vector3){0, 1, 0};
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK) {
        ecs_setCallback(
            &engine->ecs, ent, type, ENGINE_ECS_CB_TYPE_ON_UPDATE,
            engine_cbCameraOnUpdate
        );
        return ENGINE_STATUS_OK;
    }
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createMeshRenderer(
    Engine *const engine, const ECSEntityID ent,
    EngineCompTransform *transformAnchor, const EngineRenderModelID modelId
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_MESH_RENDERER;
    ECSComponent compRaw;
    EngineCompMeshRenderer *const comp = &(((EngineECSCompData*)&compRaw.data)->meshR);
    HashmapVal modelHVal;
    ECSStatus res;

    if(!hashmap_get(&engine->render.models, modelId, &modelHVal)) {
        logMsg(LOG_LVL_ERR, "model id %u not found", modelId);
        return ENGINE_STATUS_MODEL_NOT_FOUND;
    }

    Model *const mdl = (Model*)modelHVal.ptr;

    comp->boundingBox = GetModelBoundingBox(*mdl);
    comp->castShadow = 1;
    comp->color = (Vector3){1, 1, 1};
    comp->alpha = 1;
    comp->modelId = modelId;
    comp->visible = 1;
    comp->shaderId = ECS_INVALID_ID;
    
    if(transformAnchor)
        comp->transform = transformAnchor;
    else {
        res = ecs_getCompData(
            &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_TRANSFORM,
            (void**)&transformAnchor
        );
        if(res != ECS_RES_OK) {
            logMsg(
                LOG_LVL_ERR,
                "trans. anchor null and no trans. in entity id %u, model %u",
                ent, modelId
            );
            return ENGINE_STATUS_NO_TRANSFORM_ANCHOR;
        }
        comp->transform = transformAnchor;
    }

    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK) {
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_CREATE, engine_cbMeshRendererOnCreate
        ); 
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_DESTROY, engine_cbMeshRendererOnDestroy
        ); 
        return ENGINE_STATUS_OK;
    }
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createAmbientLight(
    Engine *const engine, const ECSEntityID ent, const Vector3 color
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_LIGHTSOURCE;
    ECSComponent compRaw;
    EngineCompLightSrc *const comp = &(((EngineECSCompData*)&compRaw.data)->light);
    uint8_t res;
    comp->type = ENGINE_LIGHTSRC_AMBIENT;
    comp->visible = 1;
    comp->castShadow = 0;
    comp->color = color;
    
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK) {
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_CREATE, engine_cbLightSourceOnCreate
        ); 
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_DESTROY, engine_cbLightSourceOnDestroy
        );
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_UPDATE, engine_cbLightSourceOnUpdate
        );
        return ENGINE_STATUS_OK;
    }
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createDirLight(
    Engine *const engine, const ECSEntityID ent,
    const Vector3 color, const Vector3 dir
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_LIGHTSOURCE;
    ECSComponent compRaw;
    EngineCompLightSrc *const comp = &(((EngineECSCompData*)&compRaw.data)->light);
    uint8_t res;
    comp->type = ENGINE_LIGHTSRC_DIRECTIONAL;
    comp->visible = 1;
    comp->castShadow = 1;
    comp->dir = dir;
    comp->color = color;
    
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK) {
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_CREATE, engine_cbLightSourceOnCreate
        ); 
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_DESTROY, engine_cbLightSourceOnDestroy
        );
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_UPDATE, engine_cbLightSourceOnUpdate
        );
        return ENGINE_STATUS_OK;
    }
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createPointLight(
    Engine *const engine, const ECSEntityID ent,
    EngineCompTransform *transformAnchor,
    const Vector3 color, const Vector3 pos, const float range
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_LIGHTSOURCE;
    ECSComponent compRaw;
    EngineCompLightSrc *const comp = &(((EngineECSCompData*)&compRaw.data)->light);
    uint8_t res;
    comp->type = ENGINE_LIGHTSRC_POINT;
    comp->visible = 1;
    comp->castShadow = 0;
    comp->color = color;
    comp->pos = pos;
    comp->range = range;

    if(transformAnchor == NULL) {
        res = ecs_getCompData(
            &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_TRANSFORM,
            (void**)&transformAnchor
        );
        if(res != ECS_RES_OK) {
            logMsg(
                LOG_INFO,
                "trans. anchor null and no trans. for light in entity id %u",
                ent
            );
        }
    }
    comp->transform = transformAnchor;

    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK) {
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_CREATE, engine_cbLightSourceOnCreate
        );
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_DESTROY, engine_cbLightSourceOnDestroy
        );
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_UPDATE, engine_cbLightSourceOnUpdate
        );
        return ENGINE_STATUS_OK;
    }
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createRigidBody(
    Engine *const engine, const ECSEntityID ent, const float mass
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_RIGIDBODY;
    ECSComponent compRaw;
    PhysicsRigidBody *const comp = &(((EngineECSCompData*)&compRaw.data)->rigidBody);
    *comp = physics_initRigidBody(mass);
    
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK) {
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_CREATE, engine_cbRigidBodyOnCreate
        );
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_UPDATE, engine_cbRigidBodyOnUpdate
        );
        ecs_setCallback(
            &engine->ecs, ent, type,
            ENGINE_ECS_CB_TYPE_ON_DESTROY, engine_cbRigidBodyOnDestroy
        );
        return ENGINE_STATUS_OK;
    }
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createSphereCollider(
    Engine *engine, ECSEntityID ent, float radius
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_COLLIDER;
    ECSComponent compRaw;
    Collider *const comp = &(((EngineECSCompData*)&compRaw.data)->coll);
    comp->collMask = 0xffffffff;
    comp->collTargetMask = 0;
    comp->nContacts = 0;
    comp->type = COLLIDER_TYPE_SPHERE;
    comp->sphere.radius = radius;
    
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK)
        return ENGINE_STATUS_OK;
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createConvexHullCollider(
    Engine *engine, ECSEntityID ent, float *vert, size_t nVert
) {
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_COLLIDER;
    ECSComponent compRaw;
    Collider *const comp = &(((EngineECSCompData*)&compRaw.data)->coll);
    comp->collMask = 0xffffffff;
    comp->collTargetMask = 0;
    comp->nContacts = 0;
    comp->type = COLLIDER_TYPE_CONVEX_HULL;
    comp->convexHull.vertices = vert;
    comp->convexHull.nVertices = nVert;
    
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) == ECS_RES_OK)
        return ENGINE_STATUS_OK;
    return ENGINE_STATUS_REGISTER_FAILED;
}

EngineStatus engine_createConvexHullColliderModel(
    Engine *engine, ECSEntityID ent, EngineRenderModelID id
) {
    Model *mdl = engine_render_getModel(engine, id);
    if(mdl == NULL) {
        logMsg(LOG_LVL_ERR, "model id %u not found", id);
        return ENGINE_STATUS_MODEL_NOT_FOUND;
    }
    return engine_createConvexHullCollider(
        engine, ent, mdl->meshes[0].vertices, mdl->meshes[0].vertexCount
    );
}


EngineCompInfo *engine_getInfo(Engine *const engine, const ECSEntityID ent) {
    EngineECSCompData *data;
    const ECSStatus res = ecs_getCompData(
        &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_INFO, (void**)&data
    );
    if(res != ECS_RES_OK)
        return NULL;
    return &data->info;
}

EngineCompTransform *engine_getTransform(
    Engine *const engine, const ECSEntityID ent
) {
    EngineECSCompData *data;
    const ECSStatus res = ecs_getCompData(
        &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_TRANSFORM, (void**)&data
    );
    if(res != ECS_RES_OK)
        return NULL;
    return &data->trans;
}

EngineCompCamera *engine_getCamera(
    Engine *const engine, const ECSEntityID ent
) {
    EngineECSCompData *data;
    const ECSStatus res = ecs_getCompData(
        &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_CAMERA, (void**)&data
    );
    if(res != ECS_RES_OK)
        return NULL;
    return &data->cam;
}

EngineCompMeshRenderer *engine_getMeshRenderer(
    Engine *const engine, const ECSEntityID ent
) {
    EngineECSCompData *data;
    const ECSStatus res = ecs_getCompData(
        &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_MESH_RENDERER, (void**)&data
    );
    if(res != ECS_RES_OK)
        return NULL;
    return &data->meshR;
}

EngineCompLightSrc *engine_getLightSrc(
    Engine *const engine, const ECSEntityID ent
) {
    EngineECSCompData *data;
    const ECSStatus res = ecs_getCompData(
        &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_LIGHTSOURCE, (void**)&data
    );
    if(res != ECS_RES_OK)
        return NULL;
    return &data->light;
}

PhysicsRigidBody *engine_getRigidBody(Engine *engine, ECSEntityID ent) {
    EngineECSCompData *data;
    const ECSStatus res = ecs_getCompData(
        &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_RIGIDBODY, (void**)&data
    );
    if(res != ECS_RES_OK)
        return NULL;
    return &data->rigidBody;
}

Collider *engine_getCollider(Engine *engine, ECSEntityID ent) {
    EngineECSCompData *data;
    const ECSStatus res = ecs_getCompData(
        &engine->ecs, ent, ENGINE_ECS_COMP_TYPE_COLLIDER, (void**)&data
    );
    if(res != ECS_RES_OK)
        return NULL;
    return &data->coll;
}


Vector3 engine_meshRendererCenter(const EngineCompMeshRenderer *const mr) {
    const BoundingBox *meshBB = &mr->boundingBox;
    Vector3 meshBBCenter = (Vector3) {
        mr->transform->globalMatrix.m12,
        mr->transform->globalMatrix.m13,
        mr->transform->globalMatrix.m14
    };
    meshBBCenter = Vector3Add(meshBBCenter, (Vector3) {
        (meshBB->min.x + meshBB->max.x) / 2,
        (meshBB->min.y + meshBB->max.y) / 2,
        (meshBB->min.z + meshBB->max.z) / 2
    });
    return meshBBCenter;
}