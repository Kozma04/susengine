#include "engine/physcoll.h"
#define RAYLIB_NUKLEAR_IMPLEMENTATION
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

// clang-format off
#include "moonnuklear/internal.h"
#include "raylib-nuklear/raylib-nuklear.h"
// clang-format on
#include <raygui.h>
#include <raylib.h>
#include <raymath.h>

#include "engine/ecs.h"
#include "engine/engine.h"
#include "engine/logger.h"
#include "engine/luaenv.h"
#include "engine/render.h"
#include "gamecomp.h"

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
    Mesh mesh = GenMeshCylinder(radius, height, 6);
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

void loadAssets(Engine *engine) {
    // Asset loading
    Image skyboxImg = LoadImage("assets/skybox/cubemap.png");
    Texture2D skyboxTex =
        LoadTextureCubemap(skyboxImg, CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE);
    Texture2D waterTex = LoadTexture("assets/water.png");
    SetTextureFilter(waterTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(skyboxImg);

    registerModel(engine, GAME_MODEL_CUBE, "assets/cube.obj");
    registerModelCylinder(engine, GAME_MODEL_CYLINDER, 0.5f, 1.f);
    registerModelCylinder(engine, GAME_MODEL_PLAYER_COLL, 1.f, 1.f);
    registerModelSkybox(engine, GAME_MODEL_SKYBOX, skyboxTex);

    registerShader(engine, SHADER_FORWARD_BASIC_ID,
                   "shaders/fwd_basic_vert.glsl",
                   "shaders/fwd_basic_frag.glsl");
    registerShader(engine, SHADER_SHADOWMAP_ID, "shaders/shadow_vert.glsl",
                   "shaders/shadow_frag.glsl");
    registerShader(engine, SHADER_CUBEMAP_ID, "shaders/cubemap_vert.glsl",
                   "shaders/cubemap_frag.glsl");
    registerShader(engine, SHADER_WATERPLANE_ID, "shaders/water_vert.glsl",
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
    Engine engine;
    Renderer rend;

    SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "susEngine");
    struct nk_context *ctx = InitNuklear(10);
    SetTargetFPS(80);
    DisableCursor();
    Vector2 winScale = GetWindowScaleDPI();
    SetWindowPosition(
        (GetMonitorWidth(0) - GetScreenWidth() * winScale.x) / 2,
        (GetMonitorHeight(0) - GetScreenHeight() * winScale.y) / 2 + 200);
    SetNuklearScaling(ctx, 1.4f);

    // fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    logSetHeaderThreshold("engine/ecs.c", LOG_LVL_INFO);

    engine_init(&engine);
    rend = render_init(4, 4);
    render_setupDirShadow(&rend, 20, 3, 512);
    loadAssets(&engine);

    luaEnvSetEngine(&engine);
    lua_State *L = luaEnvCreate(ctx);

    Weather weather = createWeather(&engine, (Vector3){0.1, 0.12, 0.15});
    Environment env = createEnvironment(&engine, (Vector3){0.6, 0.6, 0.6},
                                        (Vector3){1, -1, 0.8});
    // createWater(&engine, 4);
    engine_createScriptFromFile(&engine, L, env.id, "assets/scripts/game.lua");
    engine_entityPostCreate(&engine, env.id);

    Prop playerBarrel = createProp(&engine, GAME_MODEL_CYLINDER);
    engine.ecs.entDesc[playerBarrel.id].name = "PLAYERBARREL";
    playerBarrel.rb->mass = 30.f;
    playerBarrel.rb->cog = (Vector3){0, 3, 0};
    playerBarrel.rb->staticFriction = 0.8;
    playerBarrel.rb->dynamicFriction = 0.6;
    playerBarrel.coll->collMask = 2;
    playerBarrel.meshRenderer->color = (Vector3){1.2, 0.4, 1.2};
    playerBarrel.transform->scale = (Vector3){2, 6, 2};
    physics_setPosition(playerBarrel.rb, (Vector3){0, 10, 7});
    engine_entityPostCreate(&engine, playerBarrel.id);

    while (!WindowShouldClose()) {
        engine_dispatchMessages(&engine);
        luaEnvUpdate(L);
        engine_stepUpdate(&engine, GetFrameTime());
        render_updateState(&engine, &rend);
        UpdateNuklear(ctx);

        BeginDrawing();
        ClearBackground(DARKGRAY);
        render_drawScene(&engine, &rend);
        DrawNuklear(ctx);
        DrawFPS(0, 0);
        EndDrawing();

        if (IsKeyPressed(KEY_LEFT_ALT)) {
            if (IsCursorHidden())
                EnableCursor();
            else
                DisableCursor();
        }
    }

    UnloadNuklear(ctx);
    cleanup(&engine);
    CloseWindow();

    return 0;
}