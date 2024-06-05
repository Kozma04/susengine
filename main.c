#include "engine/physcoll.h"
#define RAYGUI_IMPLEMENTATION
#define USE_VSYNC

#include <fcntl.h>
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <raygui.h>
#include <raylib.h>
#include <raymath.h>

#include "engine/ecs.h"
#include "engine/engine.h"
#include "engine/logger.h"
#include "engine/luaenv.h"
#include "engine/render.h"
#include "gamecomp.h"

Engine engine;
Renderer rend;

void registerModel(Engine *engine, EngineRenderModelID id, char *fname) {
    Model *mdl = malloc(sizeof(*mdl));
    *mdl = LoadModel(fname);
    engine_render_addModel(engine, id, mdl);
}

void registerModelSkybox(Engine *engine, EngineRenderModelID id,
                         Texture2D tex) {
    Model *mdl = malloc(sizeof(*mdl));
    Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    *mdl = LoadModelFromMesh(cube);
    // Cubemap
    mdl->materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = tex;

    engine_render_addModel(engine, id, mdl);
}

void registerModelHeightmap(Engine *engine, EngineRenderModelID id,
                            Vector3 size, char *fname) {
    Image image = LoadImage(fname);
    Mesh mesh = GenMeshHeightmap(image, size);
    Model *mdl = malloc(sizeof(*mdl));
    *mdl = LoadModelFromMesh(mesh);
    engine_render_addModel(engine, id, mdl);
}

void registerModelCylinder(Engine *engine, EngineRenderModelID id, float radius,
                           float height) {
    Mesh mesh = GenMeshCylinder(radius, height, 8);
    Model *mdl = malloc(sizeof(*mdl));
    *mdl = LoadModelFromMesh(mesh);
    engine_render_addModel(engine, id, mdl);
}

void registerShader(Engine *engine, EngineShaderID id, char *fnameVert,
                    char *fnameFrag) {
    Shader *shader = malloc(sizeof(*shader));
    *shader = LoadShader(fnameVert, fnameFrag);
    engine_render_addShader(engine, id, shader);
}

void cleanup(Engine *engine) {
    ECSEntityID entId;
    logMsg(LOG_LVL_INFO, XTERM_PINK "Performing cleanup...");
    for (int32_t i = engine->ecs.nActiveEnt - 1; i >= 0; i--) {
        entId = engine->ecs.activeEnt[i];
        engine_entityDestroy(engine, entId);
    }
}

void loadAssets(void) {
    // Asset loading
    Image skyboxImg = LoadImage("assets/skybox/cubemap.png");
    Texture2D skyboxTex =
        LoadTextureCubemap(skyboxImg, CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE);
    Texture2D waterTex = LoadTexture("assets/water.png");
    SetTextureFilter(waterTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(skyboxImg);

    registerModel(&engine, GAME_MODEL_CUBE, "assets/cube.obj");
    registerModelCylinder(&engine, GAME_MODEL_CYLINDER, 1.f, 1.f);
    registerModelCylinder(&engine, GAME_MODEL_PLAYER_COLL, 1.f, 1.f);
    // registerModelHeightmap(&engine, 10, (Vector3){16, 8, 16},
    //                        "assets/pei_heightmap.png");
    registerModelSkybox(&engine, GAME_MODEL_SKYBOX, skyboxTex);

    Model mdl = LoadModelFromMesh(GenMeshPlane(800, 800, 200, 200));
    mdl.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = skyboxTex;
    mdl.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = waterTex;
    engine_render_addModel(&engine, 4, &mdl);

    Model boatMdl = LoadModel("assets/model/boat/boat2.obj");
    Texture2D boatTex = LoadTexture("assets/model/boat/boat_d.png");
    boatMdl.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = boatTex;
    engine_render_addModel(&engine, 5, &boatMdl);

    registerShader(&engine, SHADER_FORWARD_BASIC_ID,
                   "shaders/fwd_basic_vert.glsl",
                   "shaders/fwd_basic_frag.glsl");
    registerShader(&engine, SHADER_SHADOWMAP_ID, "shaders/shadow_vert.glsl",
                   "shaders/shadow_frag.glsl");
    registerShader(&engine, SHADER_CUBEMAP_ID, "shaders/cubemap_vert.glsl",
                   "shaders/cubemap_frag.glsl");
    registerShader(&engine, SHADER_WATERPLANE_ID, "shaders/water_vert.glsl",
                   "shaders/water_frag.glsl");
}

static void *luaAlloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    if (nsize == 0) {
        free(ptr);
        return NULL;
    } else
        return realloc(ptr, nsize);
}

void luaTermTick(lua_State *L) {
    char buff[256];
    int error;
    if (fgets(buff, sizeof(buff), stdin) != NULL) {
        error = luaL_loadbuffer(L, buff, strlen(buff), "line") ||
                lua_pcall(L, 0, 0, 0);
        if (error) {
            fprintf(stderr, "%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "raygui - controls test suite");
    SetTargetFPS(80);
    DisableCursor();

    lua_State *L = lua_newstate(luaAlloc, NULL);
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    engine_init(&engine);
    rend = render_init(4, 4);
    render_setupDirShadow(&rend, 20, 3, 1024);
    loadAssets();

    luaEnvSetEngine(&engine);
    luaEnvSetupBindings(L);

    Water water = createWater(&engine, 4);
    // water.meshRenderer->visible = 1;
    water.transform->pos = (Vector3){0, -5, 0};
    engine_entityPostCreate(&engine, water.id);

    /*Player player = createPlayer(&engine);
    player.controller->mode = GAME_PLAYERMODE_PHYSICAL;
    physics_setPosition(player.rb, (Vector3){20, 0, 20});
    engine_entityPostCreate(&engine, player.id);*/

    Weather weather = createWeather(&engine, (Vector3){0.1, 0.12, 0.15});
    // weather.transform->anchor = player.id;
    createEnvironment(&engine, (Vector3){0.6, 0.6, 0.6}, (Vector3){1, -1, 0.8});

    Prop playerBarrel = createProp(&engine, GAME_MODEL_CYLINDER);
    engine.ecs.entDesc[playerBarrel.id].name = "PLAYERBARREL";
    playerBarrel.rb->mass = 10.f;
    playerBarrel.meshRenderer->color = (Vector3){1.2, 0.4, 1.2};
    playerBarrel.transform->scale = (Vector3){2, 6, 2};
    physics_setPosition(playerBarrel.rb, (Vector3){0, 10, 7});
    engine_entityPostCreate(&engine, playerBarrel.id);

    // ECSComponentID cameraId;
    // ecs_getCompID(&engine.ecs, player.id, ENGINE_COMP_CAMERA, &cameraId);
    // engine_render_setCamera(&engine, cameraId);

    luaEnvLoadRun("assets/scripts/game.lua");

    while (!WindowShouldClose()) {
        luaTermTick(L);

        /*ECSEntityID id;
        if (ecs_findEntity(&engine.ecs, "cube", &id) == ECS_RES_OK) {
            RigidBody *rb = engine_getRigidBody(&engine, id);
            if (rb != NULL) {
                Vector3 v = rb->pos;
                printf("%.2f %.2f %.2f\n", v.x, v.y, v.z);
            }
        }*/

        engine_stepUpdate(&engine, GetFrameTime());
        render_updateState(&engine, &rend);

        BeginDrawing();
        ClearBackground(DARKGRAY);
        render_drawScene(&engine, &rend);

        DrawFPS(0, 0);
        EndDrawing();

        if (IsKeyPressed(KEY_LEFT_ALT)) {
            if (IsCursorHidden())
                EnableCursor();
            else
                DisableCursor();
        }
    }

    CloseWindow();

    return 0;
}