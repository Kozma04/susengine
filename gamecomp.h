#pragma once


#include <stdint.h>
#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>
#include "dsa.h"
#include "ecs.h"
#include "engine.h"
#include "render.h"
#include "logger.h"
#include "physics.h"


typedef enum GameEntTypeEnum {
    GAME_ENT_TYPE_PLAYER = 1 << 0,
    GAME_ENT_TYPE_ZOMBIE = 1 << 1,
    GAME_ENT_TYPE_INTERACTABLE = 1 << 2,
    GAME_ENT_TYPE_CONTAINER = 1 << 3,
    GAME_ENT_TYPE_STRUCTURE = 1 << 4,
    GAME_ENT_TYPE_DEVICE = 1 << 5, // only shows "[Use]"
    GAME_ENT_TYPE_VEHICLE = 1 << 6,
    GAME_ENT_TYPE_RC_VEHICLE = 1 << 7,
    GAME_ENT_TYPE_ITEM = 1 << 8,
    GAME_ENT_TYPE_PROP = 1 << 9
} GameEntType;


typedef struct Player {
    ECSEntityID id;
    EngineCompInfo *info;
    EngineCompTransform *transform;
    EngineCompCamera *camera;
} Player;

typedef struct Prop {
    ECSEntityID id;
    EngineCompInfo *info;
    EngineCompTransform *transform;
    EngineCompMeshRenderer *meshRenderer;
    PhysicsRigidBody *rb;
} Prop;

typedef struct Weather {
    ECSEntityID id;
    EngineCompInfo *info;
    EngineCompTransform *transform;
    EngineCompLightSrc *ambient;
} Weather;

typedef struct Environment {
    ECSEntityID id;
    EngineCompInfo *info;
    EngineCompTransform *transform;
    EngineCompLightSrc *dirLight;
} Environment;

typedef struct Lightbulb {
    ECSEntityID id;
    EngineCompInfo *info;
    EngineCompTransform *transform;
    EngineCompLightSrc *pointLight;
    EngineCompMeshRenderer *meshRenderer;
} Lightbulb;


Player createPlayer(Engine *engine);
Prop createProp(Engine *engine, EngineRenderModelID modelId);
Weather createWeather(Engine *engine, Vector3 ambientColor);
Environment createEnvironment(Engine *engine, Vector3 lightColor, Vector3 lightDir);
Lightbulb createLightbulb(Engine *engine, Vector3 lightColor, float range);