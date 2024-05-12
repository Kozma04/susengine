#include "render.h"


Renderer render_init(size_t maxSrcPoint, size_t maxSrcDir) {
    Renderer r;
    r.light.maxSrcPoint = maxSrcPoint;
    r.light.maxSrcDir = maxSrcDir;
    r.shadowDir.shadowMap = 0;
    r.shadowDir.nCascades = 0;
    r.shadowDir.resolution = 0;
    r.shadowDir.size = 0;
    r.state.shadowDirCam = NULL;
    r.state.meshRendVisible = array_init();
    r.state.meshRendVisibleDist = array_init();
    r.state.mainCam.position = (Vector3){0, 0, 0};
    r.state.mainCam.target = (Vector3){0, 0, 1};
    r.state.mainCam.up = (Vector3){0, 1, 0};
    r.state.mainCam.fovy = 60;
    r.state.mainCam.projection = CAMERA_PERSPECTIVE;
    return r;
}

void render_setupDirShadow(Renderer *rend, float size, size_t nCascades,
                           uint32_t res) {
    float mapSize;
    uint8_t ok = 1;
    rend->shadowDir.size = size;
    rend->shadowDir.nCascades = nCascades;
    rend->shadowDir.resolution = res;

    rend->state.shadowDirCam = calloc(nCascades, sizeof(Camera));
    rend->shadowDir.shadowMap = calloc(nCascades, sizeof(RenderTexture));
    for(int i = 0; i < nCascades && ok; i++) {
        RenderTexture *rt = rend->shadowDir.shadowMap + i;
        uint32_t rtRes = res;// / (i + 1);
        rt->id = rlLoadFramebuffer(rtRes, rtRes);
        rlEnableFramebuffer(rt->id);
        rt->depth.id = rlLoadTextureDepth(rtRes, rtRes, 0);
        rt->depth.width = res;
        rt->depth.height = res;
        rt->depth.mipmaps = 1;
        rt->depth.format = 19;
        
        rt->texture.width = res;
        rt->texture.height = res;
        rt->texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

        rlFramebufferAttach(
            rt->id,
            rt->depth.id,
            RL_ATTACHMENT_DEPTH,
            RL_ATTACHMENT_TEXTURE2D,
            0
        );
        // unbind framebuffer and also check if it is complete
        if(!rlFramebufferComplete(rt->id)) {
            ok = 0;
            logMsg(LOG_FATAL, "could not create framebuffer %u", i);
        }
        rlDisableFramebuffer();
    }
    if(ok) {
        logMsg(
            LOG_LVL_INFO, "created shadow map (%u cascades, %ux%u, %.3f base size)",
            nCascades, res, res, size
        );
    }
}


static void render_sortMeshRenderers(Engine *const engine, Renderer *const rend,
                                     Camera camera) {
    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &rend->state.meshRendVisible;
    Array *const meshRendVisDist = &rend->state.meshRendVisibleDist;
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
    Engine *const engine, Renderer *const rend, Camera cam
) {
    BoundingBox transBox;
    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &rend->state.meshRendVisible;
    const Frustum frustum = GetCameraFrustum(
        cam, (float)GetScreenWidth() / GetScreenHeight()
    );
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
    Engine *const engine, Renderer *const rend, const Shader shader
) {

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
                if(nSrcDir >= rend->light.maxSrcDir)
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
                if(nSrcPoint >= rend->light.maxSrcPoint)
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

void render_setMeshRendererUniforms(
    Engine *const engine, Renderer *const rend, const Shader shader,
    const EngineCompMeshRenderer *const mr
) {
    const Vector4 meshColor = (Vector4){
        mr->color.x, mr->color.y, mr->color.z, mr->alpha
    };
    uint32_t shadowTexSlot = 16;
    uint32_t i;

    // Assign shadow map uniforms
    for(i = 0; i < rend->shadowDir.nCascades; i++, shadowTexSlot++) {
        //glActiveTexture(GL_TEXTURE0 + shadowTexSlot);
        //glBindTexture(GL_TEXTURE_2D, rend->shadowDir.shadowMap[i].depth.id);
        rlActiveTextureSlot(shadowTexSlot);
        rlEnableTexture(rend->shadowDir.shadowMap[i].depth.id);
        SetShaderValue(
            shader,
            GetShaderLocation(shader, TextFormat("shadowMap[%u]", i)),
            &shadowTexSlot, SHADER_UNIFORM_INT
        );
        SetShaderValueMatrix(
            shader, GetShaderLocation(shader, TextFormat("shadowProj[%u]", i)),
            MatrixMultiply(
                GetViewMatrix(rend->state.shadowDirCam[i]),
                GetProjectionMatrix(rend->state.shadowDirCam[i], 1.f, 0)
            )
        );
    }
    SetShaderValue(
        shader, GetShaderLocation(shader, "nShadowMaps"),
        &rend->shadowDir.nCascades, SHADER_UNIFORM_INT
    );

    SetShaderValue(
        shader, GetShaderLocation(shader, "meshCol"), &meshColor,
        SHADER_UNIFORM_VEC4
    );
}

static void render_drawVisibleMeshes(Engine *const engine, Renderer *const rend,
                                     Shader *forceShader, uint8_t inShadowPass) {
    static Shader defaultShader;

    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &rend->state.meshRendVisible;
    Array *const meshRendVisDist = &rend->state.meshRendVisibleDist;
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
        if(forceShader == NULL) {
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
            }
            else shader = &defaultShader;
        }
        else {
            shader = forceShader;
        }
        
        if(!inShadowPass) {
            render_setShaderLightSrcUniforms(engine, rend, *shader);
            render_setMeshRendererUniforms(engine, rend, *shader, meshRendComp);
        }

        for(uint32_t j = 0; j < model->materialCount; j++)
            model->materials[j].shader = *shader;
        DrawModel(*model, (Vector3){0, 0, 0}, 1.f, RAYWHITE);
        //DrawBoundingBox(meshRendComp->_boundingBoxTrans, GREEN);
    }
}


static inline Camera renderGetLightSrcShadowCam(
    Renderer *const rend, const uint32_t cascade, const Camera cam,
    const EngineCompLightSrc *const lightDir
) {
    Camera shadowCam = cam;
    shadowCam.projection = CAMERA_ORTHOGRAPHIC;
    shadowCam.fovy = rend->shadowDir.size * (cascade * 2 + 1);
    shadowCam.target = cam.position;
    shadowCam.position = Vector3Add(
        cam.position,
        Vector3Scale(lightDir->dir, -500)
    );
    return shadowCam;
}


static void render_updateShadowMaps(
    Engine *const engine, Renderer *const rend, const Camera cam,
    const EngineCompLightSrc *const lightSrc
) {
    if(lightSrc->type != ENGINE_LIGHTSRC_DIRECTIONAL)
        return;

    static uint32_t framecnt = 0;
    framecnt++;
    //if(framecnt > 1) return;
        
    for(uint32_t i = 0; i < rend->shadowDir.nCascades; i++) {
        rend->state.shadowDirCam[i] = renderGetLightSrcShadowCam(
            rend, i, cam, lightSrc
        );
    }
}


static void render_drawShadowMaps(
    Engine *const engine, Renderer *const rend
) {
    Camera *cam = rend->state.shadowDirCam;
    Shader *shader;
    if(!hashmap_getP(&engine->render.shaders, SHADER_SHADOWMAP_ID, (void**)&shader)) {
        logMsg(LOG_LVL_ERR, "cannot find shadowmap shader");
        return;
    }
    for(uint32_t i = 0; i < rend->shadowDir.nCascades; i++, cam++) {
        render_createMeshRendererDrawList(engine, rend, *cam);
        render_sortMeshRenderers(engine, rend, *cam);

        BeginTextureMode(rend->shadowDir.shadowMap[i]);
        rlClearScreenBuffers();
        rlEnableDepthTest();
        BeginMode3D(*cam);
        render_drawVisibleMeshes(engine, rend, shader, 1);
        EndMode3D();
        EndTextureMode();
    }
}


void render_updateState(Engine *const engine, Renderer *const rend) {
    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &rend->state.meshRendVisible;
    Array *const meshRendVisDist = &rend->state.meshRendVisibleDist;
    Array *const lightSrcIdArr = &engine->render.lightSrc;

    ECSComponentID compId;
    EngineECSCompData *ecsCompData;
    EngineCompMeshRenderer *meshRendComp;
    EngineCompLightSrc *lightSrc;
    Camera mainCam;

    uint32_t i;

    compId = engine->render.camera;
    if(compId == ECS_INVALID_ID) {
        logMsg(LOG_LVL_ERR, "invalid main camera id");
        mainCam = rend->state.mainCam;
    }
    else {
        ecsCompData = (EngineECSCompData*)engine->ecs.comp[compId].data;
        mainCam = ecsCompData->cam.cam;
        rend->state.mainCam = mainCam;
    }
    
    
    for(i = 0; i < array_size(lightSrcIdArr); i++) {
        compId = array_get(lightSrcIdArr, i).u32;
        ecsCompData = (EngineECSCompData*)engine->ecs.comp[compId].data;
        lightSrc = &ecsCompData->light;

        if(!lightSrc->visible)
            continue;

        if(lightSrc->type == ENGINE_LIGHTSRC_DIRECTIONAL &&
            lightSrc->castShadow) {
            render_updateShadowMaps(engine, rend, mainCam, lightSrc);
            break;
        }
    }
}


void render_drawScene(Engine *const engine, Renderer *const rend) {
    Array *const meshRend = &engine->render.meshRend;
    Array *const meshRendVis = &rend->state.meshRendVisible;
    Array *const meshRendVisDist = &rend->state.meshRendVisibleDist;
    Array *const lightSrcIdArr = &engine->render.lightSrc;
    ECSComponentID compId;
    EngineECSCompData *ecsCompData;
    EngineCompMeshRenderer *meshRendComp;
    EngineCompLightSrc *lightSrc;
    Model *model;
    Shader *shader;
    uint8_t res;
    uint32_t i;

    if(array_capacity(meshRendVis) < array_capacity(meshRend)) {
        array_resize(meshRendVis, array_capacity(meshRend));
        array_resize(meshRendVisDist, array_capacity(meshRend));
    }

    // Draw shadows
    if(rend->shadowDir.shadowMap != NULL) {
        render_drawShadowMaps(engine, rend);
    }

    render_createMeshRendererDrawList(engine, rend, rend->state.mainCam);
    render_sortMeshRenderers(engine, rend, rend->state.mainCam);

    rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
    BeginMode3D(rend->state.mainCam);

    render_drawVisibleMeshes(engine, rend, NULL, 0);

    EndMode3D();
}