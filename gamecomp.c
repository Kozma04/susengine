#include "gamecomp.h"


const EngineEntType ALL_BOXES = 0x00000001;
const EngineMsgType BOOM = ENGINE_MSG_TYPE_INTERACT + 1;


static void game_cbPlayerControllerOnUpdate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    static const float sensitivity = 0.004;
    static const float speed = 30;
    static Vector3 forward = (Vector3){0, -0.8, 0.1};
    static int framecnt = 0;

    static Vector3 actualDeltaPos;

    framecnt++;

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

    Vector2 md = Vector2Scale(GetMouseDelta(), sensitivity);
    Vector3 right = Vector3CrossProduct(forward, (Vector3){0, 1, 0});
    if(framecnt >= 5) {
        forward = Vector3Normalize(forward);
        forward = Vector3RotateByAxisAngle(forward, (Vector3){0, -1, 0}, md.x);
        forward = Vector3RotateByAxisAngle(forward, right, -md.y);
    }

    //printf("rotation = %.3f deg\n", atan2(forward.x, forward.z) * RAD2DEG);
    float yaw = atan2(forward.x, forward.z);
    float pitch = -asin(forward.y);
    trans->rot = QuaternionFromEuler(pitch, yaw, 0);

    cam->cam.target = Vector3Add(cam->cam.position, forward);

    if(IsKeyDown(KEY_W))
        deltaPos = Vector3Add(deltaPos, forward);
    else if(IsKeyDown(KEY_S))
        deltaPos = Vector3Subtract(deltaPos, forward);
    if(IsKeyDown(KEY_D))
        deltaPos = Vector3Add(deltaPos, right);
    else if(IsKeyDown(KEY_A))
        deltaPos = Vector3Subtract(deltaPos, right);
    deltaPos.y = 0;
    if(IsKeyDown(KEY_SPACE))
        deltaPos = Vector3Add(deltaPos, (Vector3){0, 1, 0});
    else if(IsKeyDown(KEY_LEFT_SHIFT))
        deltaPos = Vector3Add(deltaPos, (Vector3){0, -1, 0});
    deltaPos = Vector3Normalize(deltaPos);
    deltaPos = Vector3Scale(deltaPos, speed * cbData->update.deltaTime);

    actualDeltaPos = Vector3Add(actualDeltaPos, Vector3Scale(
        Vector3Subtract(deltaPos, actualDeltaPos), 0.1
    ));

    trans->pos = Vector3Add(trans->pos, actualDeltaPos);
    trans->localUpdate = 1;

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
    const EngineECSCompType type = ENGINE_ECS_COMP_TYPE_USER + 1;
    ECSComponent compRaw;
    if(ecs_registerComp(&engine->ecs, ent, type, compRaw) != ECS_RES_OK) {
        logMsg(LOG_LVL_ERR, "couldn't register collision debug view component");
        return;
    }
    ecs_setCallback(
        &engine->ecs, ent, type, ENGINE_ECS_CB_TYPE_ON_UPDATE,
        engine_cbCollisionDbgViewOnUpdate
    );
}

static void boxPropCustomCallback(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    EngineCallbackData *cbData = cbUserData;
    PhysicsRigidBody *body = engine_getRigidBody(cbData->engine, entId);

    if(cbData->msgRecv.msg->msgType == BOOM) {
        logMsg(LOG_LVL_INFO, "entity %u blows up!", entId);

        Vector3 *playerPos = (Vector3*)cbData->msgRecv.msg->msgData;
        
        Vector3 diff = Vector3Subtract(body->pos, *playerPos);
        diff = Vector3Scale(diff, 4);
        diff.y = 50;

        body->vel = diff;
    }
}




Player createPlayer(Engine *engine) {
    const static char *const name = "player";
    Player player;
    ecs_registerEntity(&engine->ecs, &player.id, name);
    engine_createInfo(engine, player.id, GAME_ENT_TYPE_PLAYER);
    engine_createTransform(engine, player.id, ECS_INVALID_ID);
    engine_createCamera(engine, player.id, 75, CAMERA_PERSPECTIVE);

    ECSComponent controller;
    ecs_registerComp(
        &engine->ecs, player.id, ENGINE_ECS_COMP_TYPE_USER, controller
    );
    ecs_setCallback(
        &engine->ecs, player.id, ENGINE_ECS_COMP_TYPE_USER,
        ENGINE_ECS_CB_TYPE_ON_UPDATE, game_cbPlayerControllerOnUpdate
    );
    player.info = engine_getInfo(engine, player.id);
    player.transform = engine_getTransform(engine, player.id);
    player.camera = engine_getCamera(engine, player.id);

    engine_entityPostCreate(engine, player.id);
    return player;
}


static float cubeCollVert[] = {
    -0.5, -0.5, -0.5,
     0.5, -0.5, -0.5,
    -0.5,  0.5, -0.5,
     0.5,  0.5, -0.5,
    -0.5, -0.5,  0.5,
     0.5, -0.5,  0.5,
    -0.5,  0.5,  0.5,
     0.5,  0.5,  0.5
};


Prop createProp(Engine *engine, EngineRenderModelID modelId) {
    const static char *const name = "prop";
    Prop prop;
    ecs_registerEntity(&engine->ecs, &prop.id, name);
    engine_createInfo(engine, prop.id, GAME_ENT_TYPE_PROP);
    engine_createTransform(engine, prop.id, ECS_INVALID_ID);
    engine_createMeshRenderer(
        engine, prop.id, engine_getTransform(engine, prop.id), modelId
    );
    engine_createConvexHullCollider(engine, prop.id, cubeCollVert, 8);
    engine_createRigidBody(engine, prop.id, 1.f);
    //engine_createCollisionDbgView(engine, prop.id);

    ecs_setCallback(
        &engine->ecs, prop.id, ENGINE_ECS_COMP_TYPE_RIGIDBODY,
        ENGINE_ECS_CB_TYPE_ON_MSG_RECV, boxPropCustomCallback
    );


    prop.info = engine_getInfo(engine, prop.id);
    prop.transform = engine_getTransform(engine, prop.id);
    prop.rb = engine_getRigidBody(engine, prop.id);
    prop.meshRenderer = engine_getMeshRenderer(engine, prop.id);
    prop.meshRenderer->shaderId = SHADER_FORWARD_BASIC_ID;
    return prop;
}

Water createWater(Engine *engine, EngineRenderModelID modelId) {
    const static char *const name = "prop";
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
    ecs_setCallback(&engine->ecs, prop.id, ENGINE_ECS_COMP_TYPE_MESH_RENDERER,
                    ENGINE_ECS_CB_TYPE_PRE_RENDER, waterCbPreRender);
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

    
    engine_createMeshRenderer(engine, weather.id, engine_getTransform(engine, weather.id), 0);
    EngineCompMeshRenderer *mr = engine_getMeshRenderer(engine, weather.id);
    mr->shaderId = SHADER_CUBEMAP_ID;
    mr->castShadow = 0;
    ecs_setCallback(&engine->ecs, weather.id, ENGINE_ECS_COMP_TYPE_MESH_RENDERER,
                    ENGINE_ECS_CB_TYPE_PRE_RENDER, weatherCbPreRender);
    ecs_setCallback(&engine->ecs, weather.id, ENGINE_ECS_COMP_TYPE_MESH_RENDERER,
                    ENGINE_ECS_CB_TYPE_POST_RENDER, weatherCbPostRender);
    

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