#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <raylib.h>
#include <raymath.h>

#include "logger.h"
#include "ecs.h"
#include "engine.h"
#include "render.h"
#include "gamecomp.h"


void registerModel(Engine *engine, EngineRenderModelID id, char *fname) {
    Model *mdl = malloc(sizeof(*mdl));
    *mdl = LoadModel(fname);
    engine_render_addModel(engine, id, mdl);
}

void registerModelHeightmap(Engine *engine, EngineRenderModelID id, Vector3 size, char *fname) {
    Image image = LoadImage(fname);
    Mesh mesh = GenMeshHeightmap(image, size);
    Model *mdl = malloc(sizeof(*mdl));
    *mdl = LoadModelFromMesh(mesh);
    engine_render_addModel(engine, id, mdl);
}

void registerShader(Engine *engine, EngineShaderID id, char *fnameVert, char *fnameFrag) {
    Shader *shader = malloc(sizeof(*shader));
    *shader = LoadShader(fnameVert, fnameFrag);
    engine_render_addShader(engine, id, shader);
}

void cleanup(Engine *engine) {
    ECSEntityID entId;
    logMsg(LOG_LVL_INFO, XTERM_PINK "Performing cleanup...");
    for(int32_t i = engine->ecs.nActiveEnt - 1; i >= 0; i--) {
        entId = engine->ecs.activeEnt[i];
        engine_entityDestroy(engine, entId);
    }
}

int main(void) {
    Engine engine;
    ECSComponentID cameraId;
    Player player;
    Prop prop;
    Lightbulb light;

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(2000, 1250, "Engine");
    DisableCursor();
    //SetTargetFPS(60);

    engine_init(&engine);

    // Asset loading
    registerModel(&engine, 1, "assets/cube.obj");
    registerModelHeightmap(&engine, 2, (Vector3){16, 8, 16}, "assets/pei_heightmap.png");
    registerShader(
        &engine, GAME_SHADER_FORWARD_BASIC_ID,
        "shaders/fwd_basic_vert.glsl", "shaders/fwd_basic_frag.glsl"
    );


    // Game setup
    player = createPlayer(&engine);
    prop = createProp(&engine, 2);
    light = createLightbulb(&engine, (Vector3){1, 0.2, 0.1}, 20);
    createWeather(&engine, (Vector3){0.1, 0.12, 0.15});
    createEnvironment(&engine, (Vector3){0.3, 0.3, 0.3}, (Vector3){0.3, -1, 0.7});

    light.transform->anchor = player.id;

    player.transform->pos = (Vector3){0, 4, 0};
    prop.transform->pos = (Vector3){0, -5, 0};
    prop.transform->scale = (Vector3){10, 2, 10};

    ecs_getCompID(&engine.ecs, player.id, ENGINE_ECS_COMP_TYPE_CAMERA, &cameraId);
    engine_render_setCamera(&engine, cameraId);

    for(int x = -4; x < 4; x++) {
        for(int y = -4; y < 4; y++) {
            Prop prop = createProp(&engine, 1);
            prop.transform->pos = (Vector3){x * 3, 0, y * 3};
            prop.meshRenderer->color = (Vector3){0.7, 1, 0.7};
        }
    }


    // Game loop
    logMsg(LOG_LVL_INFO, "Running game loop");
    while(!WindowShouldClose()) {
        light.transform->pos = (Vector3){
            cos(GetTime() * 8) * 2, 0, sin(GetTime() * 8) * 2
        };
        light.transform->localUpdate = 1;

        // Update
        engine_stepUpdate(&engine, GetFrameTime());

        // Render
        BeginDrawing();

        ClearBackground(DARKGRAY);
        render_drawScene(&engine);

        DrawText(
            TextFormat("%u FPS / %.2f ms", GetFPS(), GetFrameTime() * 1000),
            0, 0, 20, LIME
        );
        DrawText(
            TextFormat("Visible Mesh Renderers: %u/%u", array_size(&engine.render.meshRendVisible), array_size(&engine.render.meshRend)),
            0, 20, 20, LIME
        );
        DrawText(
            TextFormat("Entities: %u/%u", engine.ecs.nActiveEnt, ECS_MAX_ENTITIES),
            0, 40, 20, LIME
        );
        DrawText(
            TextFormat("Components: %u/%u", fifo_av_write(&engine.ecs.freeCompId), ECS_MAX_COMPONENTS),
            0, 60, 20, LIME
        );

        EndDrawing();
    }

    cleanup(&engine);

    CloseWindow();
}