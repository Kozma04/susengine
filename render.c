#include "render.h"


static void render_sortMeshRenderers(Engine *const engine, Camera camera) {
    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &engine->render.meshRendVisible;
    Array *const meshRendVisDist = &engine->render.meshRendVisibleDist;
    Vector3 meshBBCenter;
    EngineCompMeshRenderer *meshRendComp;
    EngineECSCompData *ecsCompData;
    uint32_t i;
    uint32_t compPos;
    uint8_t sorted = 0;
    float dist;

    const Vector3 camPos = camera.position;

    // Compute distances between mesh renderers and camera
    array_clear(meshRendVisDist);
    for(i = 0; i < array_size(meshRendVis); i++) {
        compPos = array_get(meshRendVis, i).u32;
        ecsCompData = (EngineECSCompData*)engine->ecs.comp[compPos].data;
        meshRendComp = &ecsCompData->meshR;
        meshBBCenter = engine_meshRendererCenter(meshRendComp);
        dist = Vector3DistanceSqr(meshBBCenter, camPos);
        array_pushBack(meshRendVisDist, (ArrayVal)dist);
    }

    if(array_size(meshRendVis) == 0)
        return;
    
    while(!sorted) {
        sorted = 1;
        for(i = 0; i < array_size(meshRendVis) - 1; i++) {
            float distA = array_get(meshRendVisDist, i).flt;
            float distB = array_get(meshRendVisDist, i + 1).flt;
            if(distA > distB) {
                array_swap(meshRendVis, i, i + 1);
                array_swap(meshRendVisDist, i, i + 1);
                sorted = 0;
            }
        }
    }
}


static void render_createMeshRendererDrawList(
    Engine *const engine, Camera cam
) {
    BoundingBox transBox;
    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &engine->render.meshRendVisible;
    const Frustum frustum = GetCameraFrustum(cam);
    Vector3 meshBBCenter;
    EngineCompMeshRenderer *meshRendComp;
    EngineECSCompData *ecsCompData;
    uint32_t i;
    uint32_t compPos;
    uint8_t sorted = 0;
    float dist;

    array_clear(meshRendVis);
    for(i = 0; i < array_size(meshRend); i++) {
        compPos = array_get(meshRend, i).u32;
        ecsCompData = (EngineECSCompData*)engine->ecs.comp[compPos].data;
        meshRendComp = &ecsCompData->meshR;
        if(!meshRendComp->visible)
            continue;
        if(meshRendComp->transform == NULL) {
            logMsg(
                LOG_LVL_ERR, "mesh renderer %u has null transform", compPos
            );
            continue;
        }
        transBox = BoxTransform(
            meshRendComp->boundingBox,
            meshRendComp->transform->globalMatrix
        );
        meshRendComp->_boundingBoxTrans = transBox;
        
        Vector3 point = (Vector3){
            (transBox.min.x + transBox.max.x) / 2,
            (transBox.min.y + transBox.max.y) / 2,
            (transBox.min.z + transBox.max.z) / 2
        };
        
        if(FrustumBoxIntersect(frustum, transBox) == 2)
            continue;
    
        array_pushBack(meshRendVis, (ArrayVal)compPos);
    }
}


void render_setShaderLightSrcUniforms(
    Engine *const engine, const Shader shader
) {
    const static uint32_t maxSrcPoint = 16, maxSrcDir = 4;

    uint32_t i, compId;
    uint32_t nSrcPoint = 0, nSrcDir = 0;
    uint32_t lightVec, lightColor, lightRange; 
    const Array *const lightSrcIdArr = &engine->render.lightSrc;
    EngineECSCompData *ecsCompData;
    EngineCompLightSrc *lightSrc;
    Vector3 dirLightDir;

    for(i = 0; i < array_size(lightSrcIdArr); i++) {
        compId = array_get(lightSrcIdArr, i).u32;
        ecsCompData = (EngineECSCompData*)engine->ecs.comp[compId].data;
        lightSrc = &ecsCompData->light;

        if(!lightSrc->visible)
            continue;

        switch(lightSrc->type) {
            case ENGINE_LIGHTSRC_DIRECTIONAL:
                if(nSrcDir >= maxSrcDir)
                    break;
                lightVec = GetShaderLocation(
                    shader, TextFormat("lightDir[%u].dir", nSrcDir)
                );
                lightColor = GetShaderLocation(
                    shader, TextFormat("lightDir[%u].color", nSrcDir)
                );
                dirLightDir = Vector3Normalize(lightSrc->dir);
                SetShaderValue(shader, lightVec, &dirLightDir, SHADER_UNIFORM_VEC3);
                SetShaderValue(shader, lightColor, &lightSrc->color, SHADER_UNIFORM_VEC3);
                nSrcDir++;
                break;
            case ENGINE_LIGHTSRC_POINT:
                if(nSrcPoint >= maxSrcPoint)
                    break;
                lightVec = GetShaderLocation(
                    shader, TextFormat("lightPoint[%u].pos", nSrcPoint)
                );
                lightColor = GetShaderLocation(
                    shader, TextFormat("lightPoint[%u].color", nSrcPoint)
                );
                lightRange = GetShaderLocation(
                    shader, TextFormat("lightPoint[%u].range", nSrcPoint)
                );
                SetShaderValue(shader, lightVec, &lightSrc->dir, SHADER_UNIFORM_VEC3);
                SetShaderValue(shader, lightColor, &lightSrc->color, SHADER_UNIFORM_VEC3);
                SetShaderValue(shader, lightRange, &lightSrc->range, SHADER_UNIFORM_FLOAT);
                nSrcPoint++;
                break;
            case ENGINE_LIGHTSRC_AMBIENT:
                lightColor = GetShaderLocation(
                    shader, TextFormat("lightAmbient", nSrcPoint)
                );
                SetShaderValue(shader, lightColor, &lightSrc->color, SHADER_UNIFORM_VEC3);
                break;
            default:
                logMsg(LOG_LVL_ERR, "invalid light source type: %u", lightSrc->type);
                break;
        }
    }
    SetShaderValue(
        shader, GetShaderLocation(shader, "nLightDir"), &nSrcDir,
        SHADER_UNIFORM_INT
    );
    SetShaderValue(
        shader, GetShaderLocation(shader, "nLightPoint"), &nSrcPoint,
        SHADER_UNIFORM_INT
    );
}

void render_setMeshRendererUniforms(Engine *const engine, const Shader shader,
                                    const EngineCompMeshRenderer *const mr) {
    const Vector4 meshColor = (Vector4){
        mr->color.x, mr->color.y, mr->color.z, mr->alpha
    };
    SetShaderValue(
        shader, GetShaderLocation(shader, "meshCol"), &meshColor,
        SHADER_UNIFORM_VEC4
    );
}

static void render_drawVisibleMeshes(Engine *const engine) {
    static Shader defaultShader;

    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &engine->render.meshRendVisible;
    Array *const meshRendVisDist = &engine->render.meshRendVisibleDist;
    const ECSComponentID camId = engine->render.camera;
    EngineCompMeshRenderer *meshRendComp;
    ECSComponentID compId;
    Model *model;
    Shader *shader;
    uint8_t res;

    defaultShader.id = rlGetShaderIdDefault();
    defaultShader.locs = rlGetShaderLocsDefault();

    for(size_t i = 0; i < array_size(meshRendVis); i++) {
        compId = array_get(meshRendVis, i).u32;
        meshRendComp = (EngineCompMeshRenderer*)engine->ecs.comp[compId].data;
        res = hashmap_getP(&engine->render.models, meshRendComp->modelId, 
                           (void**)&model);
        if(!model) {
            logMsg(
                LOG_LVL_ERR, "model id %u for mesh renderer id %u not found",
                meshRendComp->modelId, compId
            );
            continue;
        }

        model->transform = meshRendComp->transform->globalMatrix;
        if(meshRendComp->shaderId != ECS_INVALID_ID) {
            res = hashmap_getP(
                &engine->render.shaders, meshRendComp->shaderId, (void**)&shader
            );
            if(!res) {
                logMsg(
                    LOG_LVL_ERR, "shader id %u for mesh id %u not found",
                    meshRendComp->shaderId, compId
                );
                shader = &defaultShader;
            }
            else {
                render_setShaderLightSrcUniforms(engine, *shader);
            }
        }
        else shader = &defaultShader;
        render_setMeshRendererUniforms(engine, *shader, meshRendComp);

        for(uint32_t j = 0; j < model->materialCount; j++) {
            model->materials[j].shader = *shader;
        }
        DrawModel(*model, (Vector3){0, 0, 0}, 1.f, RAYWHITE);
        //DrawBoundingBox(meshRendComp->_boundingBoxTrans, GREEN);
    }
}


void render_drawScene(Engine *const engine) {
    // TODO check if defaultShader should really remain static
    static Shader defaultShader;

    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &engine->render.meshRendVisible;
    Array *const meshRendVisDist = &engine->render.meshRendVisibleDist;
    const ECSComponentID camId = engine->render.camera;
    EngineCompMeshRenderer *meshRendComp;
    ECSComponentID compId;
    Model *model;
    Shader *shader;
    uint8_t res;

    if(camId == ECS_INVALID_ID) {
        logMsg(LOG_LVL_ERR, "invalid camera id");
        return;
    }

    const Camera cam = ((EngineECSCompData*)engine->ecs.comp[camId].data)->cam.cam;

    if(array_capacity(meshRendVis) < array_capacity(meshRend)) {
        array_resize(meshRendVis, array_capacity(meshRend));
        array_resize(meshRendVisDist, array_capacity(meshRend));
    }

    render_createMeshRendererDrawList(engine, cam);
    render_sortMeshRenderers(engine, cam);

    BeginMode3D(cam);
    render_drawVisibleMeshes(engine);
    DrawGrid(10, 10);
    EndMode3D();
}