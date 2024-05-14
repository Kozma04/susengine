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

void registerModelSkybox(Engine *engine, EngineRenderModelID id, char *fname) {
    Model *mdl = malloc(sizeof(*mdl));
    Mesh cube = GenMeshCube(50.0f, 50.0f, 50.0f);
    *mdl = LoadModelFromMesh(cube);

    Image img = LoadImage(fname);
    mdl->materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = LoadTextureCubemap(img, CUBEMAP_LAYOUT_AUTO_DETECT);    // CUBEMAP_LAYOUT_PANORAMA
    UnloadImage(img);

    engine_render_addModel(engine, id, mdl);
}

void registerModelHeightmap(Engine *engine, EngineRenderModelID id, Vector3 size, char *fname) {
    Image image = LoadImage(fname);
    Mesh mesh = GenMeshHeightmap(image, size);
    Model *mdl = malloc(sizeof(*mdl));
    *mdl = LoadModelFromMesh(mesh);
    engine_render_addModel(engine, id, mdl);
}

void registerModelCylinder(Engine *engine, EngineRenderModelID id, float radius, float height) {
    Mesh mesh = GenMeshCylinder(radius, height, 8);
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
    Renderer rend;
    ECSComponentID cameraId;
    Player player;
    Prop prop;
    Lightbulb light;

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1280, 720, "Engine");
    //ToggleFullscreen();
    //SetWindowPosition((2560 - 2000) / 2, 0);
    DisableCursor();
    log_setHeaderThreshold("ecs.c", LOG_LVL_WARN);
    //SetTargetFPS(20);

    engine_init(&engine);
    rend = render_init(16, 4);
    render_setupDirShadow(&rend, 20, 3, 512);

    // Asset loading
    registerModelHeightmap(&engine, 2, (Vector3){16, 8, 16}, "assets/pei_heightmap.png");
    registerModelSkybox(&engine, 4, "assets/skybox/panorama1.jpg");
    registerModelCylinder(&engine, 3, 1.f, 2.5f);
    registerModel(&engine, 1, "assets/cube.obj");

    registerShader(
        &engine, SHADER_FORWARD_BASIC_ID,
        "shaders/fwd_basic_vert.glsl", "shaders/fwd_basic_frag.glsl"
    );
    registerShader(
        &engine, SHADER_SHADOWMAP_ID,
        "shaders/shadow_vert.glsl", "shaders/shadow_frag.glsl"
    );


    // Game setup
    player = createPlayer(&engine);
    player.transform->pos = (Vector3){0, 8, 0};
    //prop = createProp(&engine, 2);
    //prop.transform->pos = (Vector3){0, -5, 0};
    //prop.transform->scale = (Vector3){10, 2, 10};

    //light = createLightbulb(&engine, (Vector3){1.3, 0.2, 0.1}, 50);
    Weather weather = createWeather(&engine, (Vector3){0.1, 0.12, 0.15});
    createEnvironment(&engine, (Vector3){0.3, 0.3, 0.3}, (Vector3){0.6, -1.2, 0.3});


    Prop playerBarrel = createProp(&engine, 1);
    //playerBarrel.transform->anchor = player.id;
    //playerBarrel.transform->pos = (Vector3){0, 0, 10};
    physics_setPosition(playerBarrel.rb, (Vector3){0, 3, 7});
    engine_entityPostCreate(&engine, playerBarrel.id);

    Prop plane = createProp(&engine, 1);
    physics_setPosition(plane.rb, (Vector3){0, -5, 0});
    plane.rb->mass = 0;
    plane.transform->scale = (Vector3){30, 1, 30};
    physics_setPosition(plane.rb, (Vector3){0, 0, 0});
    engine_entityPostCreate(&engine, plane.id);
    //plane.transform->pos = (Vector3){0, -5, 0};


    //light.transform->anchor = player.id;


    ecs_getCompID(&engine.ecs, player.id, ENGINE_ECS_COMP_TYPE_CAMERA, &cameraId);
    engine_render_setCamera(&engine, cameraId);

    for(int x = -1; x < 2; x++) {
        for(int y = -1; y < 2; y++) {
            Prop prop = createProp(&engine, 1);
            prop.meshRenderer->color = (Vector3){0.7, 1, 0.7};
            prop.rb->mass = 0.1f;
            prop.meshRenderer->castShadow = 1;
            physics_setPosition(prop.rb, (Vector3){x * 3, 8, y * 3});
            engine_entityPostCreate(&engine, prop.id);
            //prop.transform->pos = (Vector3){x * 5, 0, y * 5};
        }
    }

    // Game loop
    logMsg(LOG_LVL_INFO, "Running game loop");
    while(!WindowShouldClose()) {
        /*light.transform->pos = (Vector3){
            cos(GetTime() * 2) * 20, 0, sin(GetTime() * 2) * 20
        };
        light.transform->localUpdate = 1;*/

        weather.transform->pos = player.transform->pos;
        weather.transform->localUpdate = 1;


        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector3 fwd = Vector3Normalize(Vector3Subtract(
                player.camera->cam.target,
                player.camera->cam.position
            ));
            fwd = Vector3Scale(fwd, 20);
            playerBarrel.rb->accel = fwd;
        }
        else playerBarrel.rb->accel = (Vector3){0, 0, 0};


        // Update
        engine_stepUpdate(&engine, GetFrameTime());
        render_updateState(&engine, &rend);

        // Render
        BeginDrawing();

        ClearBackground(DARKGRAY);
        render_drawScene(&engine, &rend);

        DrawText(
            TextFormat("%u FPS / %.2f ms", GetFPS(), GetFrameTime() * 1000),
            0, 0, 20, LIME
        );
        DrawText(
            TextFormat(
                "Visible Mesh Renderers: %u/%u",
                array_size(&rend.state.meshRendVisible),
                array_size(&engine.render.meshRend)
            ),
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