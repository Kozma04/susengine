#include "gamecomp.h"


const EngineEntType ALL_BOXES = 0x00000001;
const EngineMsgType BOOM = ENGINE_MSG_TYPE_INTERACT + 1;


static void game_cbPlayerControllerOnUpdate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    GameCompPlayerController *pc = &((GameCompController*)comp->data)->player;

    static Vector3 actualDeltaPos;

    Vector3 deltaPos = (Vector3){0, 0, 0};
    EngineCallbackData *cbData = cbUserData;
    EngineCompCamera *cam = engine_getCamera(cbData->engine, entId);
    EngineCompTransform *trans = engine_getTransform(cbData->engine, entId);
    if(cam == NULL) {
        logMsg(LOG_LVL_FATAL, "can't get camera component");
        return;
    }
    if(trans == NULL) {
        logMsg(LOG_LVL_FATAL, "can't get transform component");
        return;
    }

    Vector2 md = Vector2Scale(GetMouseDelta(), pc->sensitivity);
    Vector3 right = Vector3CrossProduct(pc->camForward, (Vector3){0, 1, 0});
    Vector3 prevForward;
    RigidBody *rb = engine_getRigidBody(cbData->engine, entId);
    Collider *coll = engine_getCollider(cbData->engine, entId);

    pc->camForward = Vector3Normalize(pc->camForward);
    pc->camForward = Vector3RotateByAxisAngle(pc->camForward, (Vector3){0, -1, 0}, md.x);
    prevForward = pc->camForward;
    pc->camForward = Vector3RotateByAxisAngle(pc->camForward, right, -md.y);
    // Clamp camera vertical orientation
    if(Vector3DotProduct(
        Vector3CrossProduct(pc->camForward, (Vector3){0, 1, 0}),
        Vector3CrossProduct(prevForward, (Vector3){0, 1, 0})
    ) < 0) {
        Vector2 dirXZ = Vector2Normalize(
            (Vector2){prevForward.x, prevForward.z}
        );
        prevForward.x = dirXZ.x * .001f;
        prevForward.z = dirXZ.y * .001f;
        prevForward.y = prevForward.y > 0 ? 1 : -1;
        prevForward = Vector3Normalize(prevForward);
        pc->camForward = prevForward;
    }

    float yaw = atan2(pc->camForward.x, pc->camForward.z);
    float pitch = -asin(pc->camForward.y);
    //trans->rot = QuaternionFromEuler(pitch, yaw, 0);

    cam->cam.target = Vector3Add(cam->cam.position, pc->camForward);

    if(IsKeyDown(KEY_W))
        deltaPos = Vector3Add(deltaPos, pc->camForward);
    else if(IsKeyDown(KEY_S))
        deltaPos = Vector3Subtract(deltaPos, pc->camForward);
    if(IsKeyDown(KEY_D))
        deltaPos = Vector3Add(deltaPos, right);
    else if(IsKeyDown(KEY_A))
        deltaPos = Vector3Subtract(deltaPos, right);
    deltaPos.y = 0;

    if(pc->mode == GAME_PLAYERMODE_NOCLIP) {
        rb->enableDynamics = 0;

        if(IsKeyDown(KEY_SPACE))
            deltaPos = Vector3Add(deltaPos, (Vector3){0, 1, 0});
        else if(IsKeyDown(KEY_LEFT_SHIFT))
            deltaPos = Vector3Add(deltaPos, (Vector3){0, -1, 0});
        deltaPos = Vector3Normalize(deltaPos);
        deltaPos = Vector3Scale(deltaPos, pc->moveSpeed * cbData->update.deltaTime);
        
        actualDeltaPos = Vector3Add(actualDeltaPos, Vector3Scale(
            Vector3Subtract(deltaPos, actualDeltaPos), 0.1
        ));

        trans->pos = Vector3Add(trans->pos, actualDeltaPos);
        trans->localUpdate = 1;
    }
    else {
        uint8_t fast = IsKeyDown(KEY_LEFT_SHIFT) && Vector3Length(deltaPos) > .5f;
        float maxVel = fast ? 8.f : 5.f;
        float camFOV = fast ? 90.f : 75.f;

        float velY = rb->vel.y;
        rb->enableDynamics = 1;
        rb->vel = Vector3Add(rb->vel, Vector3Scale(deltaPos, 1.f));
        rb->vel.y = 0;
        if(Vector3Length(rb->vel) > maxVel)
            rb->vel = Vector3Scale(Vector3Normalize(rb->vel), maxVel);
        rb->vel.y = velY;

        if(IsKeyPressed(KEY_SPACE)) {
            if(coll->nContacts) {
                rb->pos.y += 0.1f;
                rb->vel.y = 6.26f;
            }
        }
        
        cam->cam.fovy += (camFOV - cam->cam.fovy) * 10 * cbData->update.deltaTime;
    }

    

    if(IsKeyPressed(KEY_ENTER)) {
        engine_entityBroadcastMsg(
            cbData->engine, entId, ALL_BOXES, BOOM,
            &cam->cam.position, sizeof(&cam->cam.position)
        );
    }
}


static void engine_cbCollisionDbgViewOnUpdate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    const EngineCallbackData *cbData = cbUserData;
    Collider *coll = engine_getCollider(cbData->engine, entId);
    EngineCompMeshRenderer *mr = engine_getMeshRenderer(cbData->engine, entId);

    if(coll->nContacts > 0) {
        mr->color = (Vector3){1, 0, 0};
        
        for(int i = 0; i < coll->nContacts; i++) {
            logMsg(LOG_LVL_INFO, "%u collides with %u", entId, coll->contacts[i].targetId);
        }
    }
    else {
        mr->color = (Vector3){0, 1, 0};
    }
}


static void weatherCbPreRender(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    rlDisableBackfaceCulling();
    rlDisableDepthTest();
    rlDisableDepthMask();
    EngineCallbackData *cbData = (EngineCallbackData*)cbUserData;
    EngineCompMeshRenderer *mr = &((EngineECSCompData*)comp->data)->meshR;
    Model *mdl = engine_render_getModel(cbData->engine, mr->modelId);

    int texSlot = MATERIAL_MAP_CUBEMAP;
    SetShaderValue(
        cbData->render.shader, GetShaderLocation(cbData->render.shader, "environmentMap"),
        &texSlot, SHADER_UNIFORM_INT
    );
}

static void weatherCbPostRender(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    rlEnableBackfaceCulling();
    rlEnableDepthTest();
    rlEnableDepthMask();
}

static void waterCbPreRender(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    EngineCallbackData *cbData = (EngineCallbackData*)cbUserData;
    EngineCompMeshRenderer *mr = &((EngineECSCompData*)comp->data)->meshR;
    Model *mdl = engine_render_getModel(cbData->engine, mr->modelId);
    EngineCompCamera *cam = (EngineCompCamera*)cbData->engine->ecs.comp[cbData->engine->render.camera].data;

    int texSlot = MATERIAL_MAP_CUBEMAP;
    float time = GetTime();
    SetShaderValue(
        cbData->render.shader, GetShaderLocation(cbData->render.shader, "environmentMap"),
        &texSlot, SHADER_UNIFORM_INT
    );
    texSlot = MATERIAL_MAP_DIFFUSE;
    SetShaderValue(
        cbData->render.shader, GetShaderLocation(cbData->render.shader, "texDiffuse"),
        &texSlot, SHADER_UNIFORM_INT
    );
    SetShaderValue(
        cbData->render.shader, GetShaderLocation(cbData->render.shader, "time"),
        &time, SHADER_UNIFORM_FLOAT
    );
    SetShaderValue(
        cbData->render.shader, GetShaderLocation(cbData->render.shader, "cameraPosition"),
        &cam->cam.position, SHADER_UNIFORM_VEC3
    );
}

static void engine_createCollisionDbgView(
    Engine *const engine, const ECSEntityID ent
) {
    const EngineECSCompType type = ENGINE_COMP_USER + 1;
    ECSComponent compRaw;
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) != ECS_RES_OK) {
        logMsg(LOG_LVL_ERR, "couldn't register collision debug view component");
        return;
    }
    ecs_setCallback(
        &engine->ecs, ent, type, ENGINE_CB_UPDATE,
        engine_cbCollisionDbgViewOnUpdate
    );
}

static void boxPropCustomCallback(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    EngineCallbackData *cbData = cbUserData;
    RigidBody *body = engine_getRigidBody(cbData->engine, entId);

    if(cbData->msgRecv.msg->msgType == BOOM) {
        logMsg(LOG_LVL_INFO, "entity %u blows up!", entId);

        Vector3 *playerPos = (Vector3*)cbData->msgRecv.msg->msgData;
        
        Vector3 diff = Vector3Subtract(body->pos, *playerPos);
        diff = Vector3Scale(diff, GetRandomValue(10, 30) / 10.f);
        diff.y = GetRandomValue(0, 30);

        body->vel = diff;
    }
}


static void gameCreatePlayerController(Engine *engine, ECSEntityID ent) {
    ECSComponent controller;
    GameCompController *ctrl = (GameCompController*)controller.data;
    ctrl->type = GAME_CONTROLLER_PLAYER;
    ctrl->player.camForward = (Vector3){0.f, -.8f, .1f};
    ctrl->player.sensitivity = .004f;
    ctrl->player.moveSpeed = 30.f;
    ctrl->player.mode = GAME_PLAYERMODE_PHYSICAL;

    ecs_registerComp(
        &engine->ecs, ent, GAME_COMP_CONTROLLER, controller
    );
    ecs_setCallback(
        &engine->ecs, ent, GAME_COMP_CONTROLLER,
        ENGINE_CB_UPDATE, game_cbPlayerControllerOnUpdate
    );
}

Player createPlayer(Engine *engine) {
    const static char *const name = "player";
    Player player;
    ecs_registerEntity(&engine->ecs, &player.id, name);
    engine_createInfo(engine, player.id, GAME_ENT_TYPE_PLAYER);
    engine_createTransform(engine, player.id, ECS_INVALID_ID);
    engine_createCamera(engine, player.id, 75, CAMERA_PERSPECTIVE);
    engine_createConvexHullColliderModel(engine, player.id, GAME_MODEL_CYLINDER);
    engine_createRigidBody(engine, player.id, 1.f);
    gameCreatePlayerController(engine, player.id);
    
    player.info = engine_getInfo(engine, player.id);
    player.transform = engine_getTransform(engine, player.id);
    player.camera = engine_getCamera(engine, player.id);
    player.rb = engine_getRigidBody(engine, player.id);
    player.coll = engine_getCollider(engine, player.id);

    player.rb->dynamicFriction = .85f;
    player.rb->mass = 1.f;
    player.rb->enableRot = (Vector3){0, 0, 0};
    player.rb->bounce = 0.f;
    player.coll->localTransform = MatrixMultiply(
        MatrixScale(1, 1.8, 1), MatrixTranslate(0, -1.8, 0)
    );

    return player;
}

Prop createProp(Engine *engine, EngineRenderModelID modelId) {
    const static char *const name = "prop";
    Prop prop;
    ecs_registerEntity(&engine->ecs, &prop.id, name);
    engine_createInfo(engine, prop.id, GAME_ENT_TYPE_PROP);
    engine_createTransform(engine, prop.id, ECS_INVALID_ID);
    engine_createMeshRenderer(
        engine, prop.id, engine_getTransform(engine, prop.id), modelId
    );
    engine_createConvexHullColliderModel(engine, prop.id, modelId);
    engine_createRigidBody(engine, prop.id, 1.f);

    ecs_setCallback(
        &engine->ecs, prop.id, ENGINE_COMP_RIGIDBODY,
        ENGINE_CB_MSGRECV, boxPropCustomCallback
    );


    prop.info = engine_getInfo(engine, prop.id);
    prop.transform = engine_getTransform(engine, prop.id);
    prop.rb = engine_getRigidBody(engine, prop.id);
    prop.coll = engine_getCollider(engine, prop.id);
    prop.meshRenderer = engine_getMeshRenderer(engine, prop.id);
    prop.meshRenderer->shaderId = SHADER_FORWARD_BASIC_ID;
    return prop;
}

Water createWater(Engine *engine, EngineRenderModelID modelId) {
    const static char *const name = "water";
    Water prop;
    ecs_registerEntity(&engine->ecs, &prop.id, name);
    engine_createInfo(engine, prop.id, GAME_ENT_TYPE_PROP);
    engine_createTransform(engine, prop.id, ECS_INVALID_ID);
    engine_createMeshRenderer(
        engine, prop.id, engine_getTransform(engine, prop.id), modelId
    );
    //engine_createConvexHullColliderModel(engine, prop.id, modelId);
    //engine_createCollisionDbgView(engine, prop.id);
    prop.info = engine_getInfo(engine, prop.id);
    prop.transform = engine_getTransform(engine, prop.id);
    prop.meshRenderer = engine_getMeshRenderer(engine, prop.id);
    prop.meshRenderer->shaderId = SHADER_WATERPLANE_ID;
    prop.meshRenderer->castShadow = 0;
    prop.meshRenderer->alpha = .4f;
    prop.meshRenderer->distanceMode = RENDER_DIST_MIN;
    ecs_setCallback(&engine->ecs, prop.id, ENGINE_COMP_MESHRENDERER,
                    ENGINE_CB_PRERENDER, waterCbPreRender);
    //ecs_setCallback(&engine->ecs, prop.id, ENGINE_ECS_COMP_TYPE_MESH_RENDERER,
    //                ENGINE_ECS_CB_TYPE_POST_RENDER, waterCbPostRender);
    return prop;
}

Weather createWeather(Engine *engine, Vector3 ambientColor) {
    const static char *const name = "weather";
    Weather weather;
    ecs_registerEntity(&engine->ecs, &weather.id, name);
    engine_createInfo(engine, weather.id, 0);
    engine_createTransform(engine, weather.id, ECS_INVALID_ID);
    engine_createAmbientLight(engine, weather.id, ambientColor);
    weather.info = engine_getInfo(engine, weather.id);
    weather.ambient = engine_getLightSrc(engine, weather.id);
    weather.transform = engine_getTransform(engine, weather.id);
    
    engine_createMeshRenderer(engine, weather.id, engine_getTransform(engine, weather.id), GAME_MODEL_SKYBOX);
    EngineCompMeshRenderer *mr = engine_getMeshRenderer(engine, weather.id);
    mr->shaderId = SHADER_CUBEMAP_ID;
    mr->castShadow = 0;
    mr->distanceMode = RENDER_DIST_MAX;
    ecs_setCallback(&engine->ecs, weather.id, ENGINE_COMP_MESHRENDERER,
                    ENGINE_CB_PRERENDER, weatherCbPreRender);
    ecs_setCallback(&engine->ecs, weather.id, ENGINE_COMP_MESHRENDERER,
                    ENGINE_CB_POSTRENDER, weatherCbPostRender);

    engine_entityPostCreate(engine, weather.id);
    return weather;
}

Environment createEnvironment(Engine *engine, Vector3 lightColor, Vector3 lightDir) {
    const static char *const name = "environment";
    Environment env;
    ecs_registerEntity(&engine->ecs, &env.id, name);
    engine_createInfo(engine, env.id, 0);
    engine_createTransform(engine, env.id, ECS_INVALID_ID);
    engine_createDirLight(engine, env.id, lightColor, lightDir);
    env.info = engine_getInfo(engine, env.id);
    env.dirLight = engine_getLightSrc(engine, env.id);
    env.transform = engine_getTransform(engine, env.id);
    engine_entityPostCreate(engine, env.id);
    return env;
}

Lightbulb createLightbulb(Engine *engine, Vector3 lightColor, float range) {
    const static char *const name = "lightbulb";
    Lightbulb ent;
    ecs_registerEntity(&engine->ecs, &ent.id, name);
    engine_createInfo(engine, ent.id, 0);
    engine_createTransform(engine, ent.id, ECS_INVALID_ID);
    engine_createPointLight(engine, ent.id, engine_getTransform(engine, ent.id), lightColor, (Vector3){0, 0, 0}, range);
    engine_createMeshRenderer(
        engine, ent.id, engine_getTransform(engine, ent.id), 1
    );
    ent.info = engine_getInfo(engine, ent.id);
    ent.pointLight = engine_getLightSrc(engine, ent.id);
    ent.transform = engine_getTransform(engine, ent.id);
    ent.meshRenderer = engine_getMeshRenderer(engine, ent.id);
    ent.meshRenderer->shaderId = SHADER_FORWARD_BASIC_ID;
    ent.meshRenderer->color = lightColor;
    engine_entityPostCreate(engine, ent.id);
    return ent;
}