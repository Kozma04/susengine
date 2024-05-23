#pragma once


#include <stdint.h>
#include <stdlib.h>
#include <raylib.h>
#include <raymath.h>
#include "engine/dsa.h"
#include "engine/ecs.h"
#include "engine/engine.h"
#include "engine/render.h"
#include "engine/logger.h"
#include "engine/physcoll.h"


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

typedef enum GameCompTypeEnum {
    GAME_COMP_CONTROLLER = ENGINE_COMP_USER
} GameCompType;

typedef enum GameControllerTypeEnum {
    GAME_CONTROLLER_PLAYER
} GameControllerType;

typedef enum GamePlayerModeEnum {
    GAME_PLAYERMODE_NOCLIP,
    GAME_PLAYERMODE_PHYSICAL
} GamePlayerMode;

enum {
    GAME_MODEL_SKYBOX,
    GAME_MODEL_CYLINDER,
    GAME_MODEL_CUBE,
    GAME_MODEL_PLAYER_COLL
};


typedef struct GameCompPlayerController {
    float sensitivity;
    float moveSpeed;
    Vector3 camForward;
    GamePlayerMode mode;
} GameCompPlayerController;

typedef struct GameCompController {
    GameControllerType type;
    union {
        GameCompPlayerController player;
    };
} GameCompController;


typedef struct Player {
    ECSEntityID id;
    EngineCompInfo *info;
    EngineCompTransform *transform;
    EngineCompCamera *camera;
    GameCompPlayerController *playerCtrl;
    RigidBody *rb;
    Collider *coll;
    GameCompPlayerController *controller;
} Player;

typedef struct Prop {
    ECSEntityID id;
    EngineCompInfo *info;
    EngineCompTransform *transform;
    EngineCompMeshRenderer *meshRenderer;
    RigidBody *rb;
    Collider *coll;
} Prop;

typedef struct Water {
    ECSEntityID id;
    EngineCompInfo *info;
    EngineCompTransform *transform;
    EngineCompMeshRenderer *meshRenderer;
} Water;

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
Water createWater(Engine *engine, EngineRenderModelID modelId);
Weather createWeather(Engine *engine, Vector3 ambientColor);
Environment createEnvironment(Engine *engine, Vector3 lightColor, Vector3 lightDir);
Lightbulb createLightbulb(Engine *engine, Vector3 lightColor, float range);