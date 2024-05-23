#pragma once

#define ENGINE_MAX_PENDING_MESSAGES 128
#define ENGINE_MESSAGE_DATA_SIZE 64

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>

#include "./dsa.h"
#include "./ecs.h"
#include "./logger.h"
#include "./physcoll.h"

typedef uint32_t EngineRenderModelID;
typedef uint32_t EngineShaderID;
typedef uint32_t EngineEntType;

typedef struct Engine Engine;

// Messaging system
typedef enum EngineMsgTypeEnum { ENGINE_MSG_TYPE_INTERACT } EngineMsgType;

typedef enum RenderDistModeEnum {
    RENDER_DIST_MIN,
    RENDER_DIST_MAX,
    RENDER_DIST_FROM_CAMERA
} RenderDistMode;

typedef enum RenderPassEnum { RENDER_PASS_SHADOW, RENDER_PASS_BASE } RenderPass;

typedef struct EngineMsg {
    ECSEntityID srcId;
    ECSEntityID dstId;
    EngineEntType dstMask;
    EngineMsgType msgType;
    uint8_t msgData[ENGINE_MESSAGE_DATA_SIZE];
} EngineMsg;

// Components system
typedef struct EngineCompInfo {
    EngineEntType typeMask;
} EngineCompInfo;

typedef struct EngineCompTransform {
    ECSEntityID anchor;

    Vector3 pos;
    Vector3 scale;
    Quaternion rot;

    uint8_t localUpdate;
    uint8_t _globalUpdate;
    Matrix localMatrix;
    Matrix globalMatrix;
} EngineCompTransform;

typedef struct EngineCompCamera {
    Camera cam;
} EngineCompCamera;

typedef struct EngineCompMeshRenderer {
    EngineCompTransform *transform; // Must not be null
    uint8_t visible;
    uint8_t castShadow;
    float alpha;
    Vector3 color;
    Vector3 emissive;
    BoundingBox boundingBox;
    EngineRenderModelID modelId;
    EngineShaderID shaderId;
    RenderDistMode distanceMode;
    BoundingBox _boundingBoxTrans;
} EngineCompMeshRenderer;

typedef enum EngineLightSrcTypeEnum {
    ENGINE_LIGHTSRC_POINT,
    ENGINE_LIGHTSRC_DIRECTIONAL,
    ENGINE_LIGHTSRC_AMBIENT
} EngineLightSrcType;

typedef struct EngineCompLightSrc {
    // Point light position anchor, can be null
    EngineCompTransform *transform;
    uint8_t visible;
    Vector3 color;
    EngineLightSrcType type;
    union {
        Vector3 pos;
        Vector3 dir;
    };
    float range;
    uint8_t castShadow;
} EngineCompLightSrc;

typedef struct EngineCallbackData {
    Engine *engine;
    union {
        struct {
            float deltaTime;
        } update;
        struct {
            EngineMsg *msg;
        } msgRecv;
        struct {
            Shader shader;
            Camera cam;
            RenderPass pass;
        } render;
    };
} EngineCallbackData;

// Entity Component system interface
typedef enum EngineECSCompTypeEnum {
    ENGINE_COMP_INFO,
    ENGINE_COMP_RIGIDBODY,
    ENGINE_COMP_TRANSFORM,
    ENGINE_COMP_CAMERA,
    ENGINE_COMP_MESHRENDERER,
    ENGINE_COMP_LIGHTSOURCE,
    ENGINE_COMP_COLLIDER,
    ENGINE_COMP_USER
} EngineECSCompType;

typedef enum EngineECSCallbackTypeEnum {
    // Entity lifecycle
    ENGINE_CB_CREATE,
    ENGINE_CB_UPDATE,
    ENGINE_CB_DESTROY,
    // Communication between components
    ENGINE_CB_MSGRECV,
    // Rendering
    ENGINE_CB_PRERENDER,
    ENGINE_CB_POSTRENDER
} EngineECSCallbackType;

typedef union EngineECSCompData {
    EngineCompInfo info;
    EngineCompTransform trans;
    EngineCompCamera cam;
    EngineCompMeshRenderer meshR;
    EngineCompLightSrc light;
    RigidBody rigidBody;
    Collider coll;
} EngineECSCompData;

typedef struct Engine {
    float timescale;
    size_t nPendingMsg;
    EngineMsg pendingMsg[ENGINE_MAX_PENDING_MESSAGES];
    ECS ecs;
    PhysicsSystem phys;
    struct {
        Hashmap models;  // Model* values
        Hashmap shaders; // Shader* values

        Array meshRend; // ComponentID (for Mesh Renderer) values
        // Array meshRendVisible; // Visible Mesh Renderer component IDs
        // Array meshRendVisibleDist; // distance to each Mesh Renderer

        Array lightSrc;        // ComponentID (for light sources) values
        ECSComponentID camera; // Camera component
    } render;
} Engine;

typedef enum EngineStatusEnum {
    ENGINE_STATUS_OK,
    ENGINE_STATUS_NO_TRANSFORM_ANCHOR,
    ENGINE_STATUS_MODEL_NOT_FOUND,
    ENGINE_STATUS_REGISTER_FAILED,
    ENGINE_STATUS_RENDER_DUPLICATE_ITEM,
    ENGINE_STATUS_RENDER_ITEM_NOT_FOUND,
    ENGINE_STATUS_MSG_PENDING_FULL,
    ENGINE_STATUS_MSG_DATA_SIZE_EXCEEDED,
    ENGINE_STATUS_MSG_SRC_NOT_FOUND,
    ENGINE_STATUS_MSG_DST_NOT_FOUND,
} EngineStatus;

// Initialize engine
void engine_init(Engine *const engine);

/* General entity management */
// Run component callbacks right after fully registering a entity to the ECS
void engine_entityPostCreate(Engine *engine, ECSEntityID id);
// Run destroy callback on the components and unregister their parent entity
void engine_entityDestroy(Engine *engine, ECSEntityID id);
// Update scene
void engine_stepUpdate(Engine *engine, float deltaTime);

/* Graphics */
// Assign Raylib Model it to a EngineRenderModelID
EngineStatus engine_render_addModel(Engine *engine, EngineRenderModelID id,
                                    const Model *model);
// Get Raylib Model from the registered models list
Model *engine_render_getModel(Engine *engine, EngineRenderModelID id);
// Assign Raylib Model it to a EngineRenderModelID
EngineStatus engine_render_addShader(Engine *engine, EngineShaderID id,
                                     const Shader *shader);
// Get shader for input id. If id is invalid, it returns the default shader.
Shader engine_render_getShader(Engine *engine, EngineShaderID id);
// Register a Mesh Renderer component to the renderer
/*EngineStatus engine_render_registerMeshRenderer(
    Engine *engine, ECSEntityID id
);
// Unregister a Mesh Renderer component from the renderer
EngineStatus engine_render_unregisterMeshRenderer(
    Engine *engine, ECSEntityID id
);*/
// Register a Light Source component to the renderer
EngineStatus engine_render_registerLightSrc(Engine *engine, ECSComponentID id);
// Unregister a Light Source component from the renderer
EngineStatus engine_render_unregisterLightSrc(Engine *engine,
                                              ECSComponentID id);
// Set Camera component which will be used for rendering the scene
void engine_render_setCamera(Engine *engine, ECSComponentID id);
// Render scene
void engine_stepRender(Engine *engine);

// Send a message from an entity to another
EngineStatus engine_entitySendMsg(Engine *engine, ECSEntityID src,
                                  ECSEntityID dst, EngineMsgType msgtype,
                                  const void *msgData, size_t msgSize);
// Send a message from an entity across all entities
EngineStatus engine_entityBroadcastMsg(Engine *engine, ECSEntityID src,
                                       EngineEntType filter,
                                       EngineMsgType msgtype,
                                       const void *msgData, size_t msgSize);

// Create and initialize Info component
EngineStatus engine_createInfo(Engine *engine, ECSEntityID ent,
                               EngineEntType entTypeMask);
// Create and initialize Transform component
EngineStatus engine_createTransform(Engine *engine, ECSEntityID ent,
                                    ECSEntityID transformAnchor);
// Create and initialize Camera component
EngineStatus engine_createCamera(Engine *engine, ECSEntityID ent, float fov,
                                 int projection);
// Create and initialize Mesh Renderer component
EngineStatus engine_createMeshRenderer(Engine *engine, ECSEntityID ent,
                                       EngineCompTransform *transformAnchor,
                                       EngineRenderModelID modelId);
// Create and initialize Light Source component of Ambient type
EngineStatus engine_createAmbientLight(Engine *engine, ECSEntityID ent,
                                       Vector3 color);
// Create and initialize Light Source component of Directional type
EngineStatus engine_createDirLight(Engine *engine, ECSEntityID ent,
                                   Vector3 color, Vector3 dir);
// Create and initialize Light Source component of Point type
EngineStatus engine_createPointLight(Engine *engine, ECSEntityID ent,
                                     EngineCompTransform *transformAnchor,
                                     Vector3 color, Vector3 pos, float range);

// Create and initialize Rigid Body component for physics simulation
EngineStatus engine_createRigidBody(Engine *engine, ECSEntityID ent,
                                    float mass);
// Create and initialize Collider component of Sphere type
EngineStatus engine_createSphereCollider(Engine *engine, ECSEntityID ent,
                                         float radius);
// Create and initialize Collider component of Convex Hull type
EngineStatus engine_createConvexHullCollider(Engine *engine, ECSEntityID ent,
                                             short *ind, float *vert,
                                             size_t nVert);
// Create and initialize Collider component of Convex Hull type from model ID
EngineStatus engine_createConvexHullColliderModel(Engine *engine,
                                                  ECSEntityID ent,
                                                  EngineRenderModelID id);

// Get Info component
EngineCompInfo *engine_getInfo(Engine *engine, ECSEntityID ent);
// Get Transform component
EngineCompTransform *engine_getTransform(Engine *engine, ECSEntityID ent);
// Get Camera component
EngineCompCamera *engine_getCamera(Engine *engine, ECSEntityID ent);
// Get Mesh Renderer component
EngineCompMeshRenderer *engine_getMeshRenderer(Engine *engine, ECSEntityID id);
// Get Light Source component
EngineCompLightSrc *engine_getLightSrc(Engine *engine, ECSEntityID id);

// Get Rigid Body component
RigidBody *engine_getRigidBody(Engine *engine, ECSEntityID id);
// Get Collider component
Collider *engine_getCollider(Engine *engine, ECSEntityID id);

// Returns the center of the bounding box of a Mesh Renderer, also considering
// its transform
Vector3 engine_meshRendererCenter(const EngineCompMeshRenderer *mr);