#include "gamecomp.h"


static void game_cbPlayerControllerOnUpdate(
    uint32_t cbType, ECSEntityID entId, ECSComponentID compId,
    uint32_t compType, struct ECSComponent *comp, void *cbUserData
) {
    static const float sensitivity = 0.004;
    static const float speed = 10;
    static Vector3 forward = {0, 0, 1};

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
    forward = Vector3RotateByAxisAngle(forward, (Vector3){0, -1, 0}, md.x);
    forward = Vector3RotateByAxisAngle(forward, right, -md.y);

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

    trans->pos = Vector3Add(trans->pos, deltaPos);
    trans->localUpdate = 1;
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

Prop createProp(Engine *engine, EngineRenderModelID modelId) {
    const static char *const name = "prop";
    Prop prop;
    ecs_registerEntity(&engine->ecs, &prop.id, name);
    engine_createInfo(engine, prop.id, GAME_ENT_TYPE_PROP);
    engine_createTransform(engine, prop.id, ECS_INVALID_ID);
    engine_createMeshRenderer(
        engine, prop.id, engine_getTransform(engine, prop.id), modelId
    );
    prop.info = engine_getInfo(engine, prop.id);
    prop.transform = engine_getTransform(engine, prop.id);
    prop.meshRenderer = engine_getMeshRenderer(engine, prop.id);
    prop.meshRenderer->shaderId = GAME_SHADER_FORWARD_BASIC_ID;
    engine_entityPostCreate(engine, prop.id);
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
    ent.meshRenderer->shaderId = GAME_SHADER_FORWARD_BASIC_ID;
    ent.meshRenderer->color = lightColor;
    engine_entityPostCreate(engine, ent.id);
    return ent;
}