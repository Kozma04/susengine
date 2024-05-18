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

void registerModelSkybox(Engine *engine, EngineRenderModelID id, Texture2D tex) {
    Model *mdl = malloc(sizeof(*mdl));
    Mesh cube = GenMeshCube(200.0f, 200.0f, 200.0f);
    *mdl = LoadModelFromMesh(cube);
    //Cubemap
    mdl->materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = tex;

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


    Engine engine;
int main(void) {
    Renderer rend;
    ECSComponentID cameraId;
    Player player;
    Prop prop;
    Lightbulb light;

    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_HIGHDPI);
    InitWindow(1280, 720, "who needs OOP anyway?");
    //ToggleFullscreen();
    //SetWindowPosition((2560 - 2000) / 2, 0);
    DisableCursor();
    log_setHeaderThreshold("ecs.c", LOG_LVL_WARN);
    SetTargetFPS(60);

    engine_init(&engine);
    rend = render_init(16, 4);
    render_setupDirShadow(&rend, 20, 3, 512);

    // Asset loading
    Image skyboxImg = LoadImage("assets/skybox/cubemap.png");
    Texture2D skyboxTex = LoadTextureCubemap(skyboxImg, CUBEMAP_LAYOUT_CROSS_FOUR_BY_THREE);
    Texture2D waterTex = LoadTexture("assets/water.png");
    SetTextureFilter(waterTex, TEXTURE_FILTER_BILINEAR);
    UnloadImage(skyboxImg);

    registerModelSkybox(&engine, 0, skyboxTex);
    registerModel(&engine, 1, "assets/cube.obj");
    registerModelHeightmap(&engine, 2, (Vector3){16, 8, 16}, "assets/pei_heightmap.png");
    registerModelCylinder(&engine, 3, 1.f, 2.5f);
    
    Model mdl = LoadModelFromMesh(GenMeshPlane(800, 800, 200, 200));
    mdl.materials[0].maps[MATERIAL_MAP_CUBEMAP].texture = skyboxTex;
    mdl.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = waterTex;
    engine_render_addModel(&engine, 4, &mdl);

    Model boatMdl = LoadModel("assets/model/boat/boat2.obj");
    Texture2D boatTex = LoadTexture("assets/model/boat/boat_d.png");
    boatMdl.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = boatTex;
    engine_render_addModel(&engine, 5, &boatMdl);

    Water water = createWater(&engine, 4);
    water.meshRenderer->visible = 1;
    water.transform->pos = (Vector3){0, -5, 0};
    engine_entityPostCreate(&engine, water.id);

    registerShader(
        &engine, SHADER_FORWARD_BASIC_ID,
        "shaders/fwd_basic_vert.glsl", "shaders/fwd_basic_frag.glsl"
    );
    registerShader(
        &engine, SHADER_SHADOWMAP_ID,
        "shaders/shadow_vert.glsl", "shaders/shadow_frag.glsl"
    );
    registerShader(
        &engine, SHADER_CUBEMAP_ID,
        "shaders/cubemap_vert.glsl", "shaders/cubemap_frag.glsl"
    );
    registerShader(
        &engine, SHADER_WATERPLANE_ID,
        "shaders/water_vert.glsl", "shaders/water_frag.glsl"
    );


    // Game setup
    player = createPlayer(&engine);
    player.transform->pos = (Vector3){0, 8, 0};

    //light = createLightbulb(&engine, (Vector3){1.3, 0.2, 0.1}, 50);

    Weather weather = createWeather(&engine, (Vector3){0.1, 0.12, 0.15});
    createEnvironment(&engine, (Vector3){0.6, 0.6, 0.6}, (Vector3){1, -1, 0.8});


    Prop playerBarrel = createProp(&engine, 3);
    engine.ecs.entDesc[playerBarrel.id].name = "PLAYERBARREL";
    playerBarrel.rb->mass = 10.f;
    playerBarrel.meshRenderer->color = (Vector3){1.2, 0.4, 1.2};
    playerBarrel.transform->scale = (Vector3){3, 3, 3};
    physics_setPosition(playerBarrel.rb, (Vector3){0, 3, 7});
    engine_entityPostCreate(&engine, playerBarrel.id);

    Prop plane = createProp(&engine, 1);
    plane.meshRenderer->color = (Vector3){1.3f, 0.9f, 0.6f};
    physics_setPosition(plane.rb, (Vector3){0, -5, 0});
    plane.rb->mass = 0;
    plane.rb->enableRot = (Vector3){0, 0, 0};
    plane.transform->scale = (Vector3){30, 1, 30};
    physics_setPosition(plane.rb, (Vector3){0, 0, 0});
    engine_entityPostCreate(&engine, plane.id);


    Prop heavyProp = createProp(&engine, 5);
    heavyProp.transform->scale = (Vector3){8, 8, 8};
    heavyProp.rb->mass = 20;
    physics_setPosition(heavyProp.rb, (Vector3){0, 0, 30});
    engine_entityPostCreate(&engine, heavyProp.id);


    ecs_getCompID(&engine.ecs, player.id, ENGINE_ECS_COMP_TYPE_CAMERA, &cameraId);
    engine_render_setCamera(&engine, cameraId);
    
    // Boring constants
    const EngineRenderModelID MODEL_CUBE = 1;
    const uint32_t CAN_EXPLODE = 1;
    const Vector3 COLOR_BLUE = (Vector3){0.7, 0.7, 1};

    // Initialize the game entity
    Prop cube = createProp(&engine, MODEL_CUBE); // Ask for a 3D model
    cube.info->typeMask = CAN_EXPLODE;           // Respond to BOOM message
    cube.rb->mass = 0.1f;                        // Rigid body mass
    cube.rb->bounce = 1.0f;                      // Rigid body elasticity
    cube.rb->pos = (Vector3){0, 20, 0};          // Rigid body position
    cube.meshRenderer->color = COLOR_BLUE;       // 3D model color
    cube.transform->scale = (Vector3){1, 1, 1};  // 3D model size
    // Pass it to the game engine
    engine_entityPostCreate(&engine, cube.id);
    

    
    for(int x = -4; x < 4; x++) {
        for(int y = -4; y < 4; y++) {
            Prop prop = createProp(&engine, 1);
            prop.info->typeMask = 0x00000001;

            prop.meshRenderer->color = (Vector3){0.7, 1, 0.7};
            prop.rb->mass = 1.f;
            prop.rb->bounce = GetRandomValue(1, 20) / 10.f;
            prop.rb->enableRot = (Vector3){1, 1, 1};
            prop.meshRenderer->castShadow = 1;
            prop.transform->scale = (Vector3){2, 2, 2};
            physics_setPosition(prop.rb, (Vector3){x * 3, 15, y * 3});
            engine_entityPostCreate(&engine, prop.id);
        }
    }

    // Game loop
    logMsg(LOG_LVL_INFO, "Running game loop");

    while(!WindowShouldClose()) {

        weather.transform->pos = player.transform->pos;
        weather.transform->localUpdate = 1;


        if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector3 fwd = Vector3Normalize(Vector3Subtract(
                player.camera->cam.target,
                player.camera->cam.position
            ));
            fwd = Vector3Scale(fwd, 20);
            if(IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
                fwd = Vector3Negate(fwd);
            playerBarrel.rb->accel = fwd;
        }
        else playerBarrel.rb->accel = (Vector3){0, 0, 0};
        Vector3 pos = playerBarrel.transform->pos;
        //logMsg(LOG_LVL_DEBUG, "player: %.2f %.2f %.2f", pos.x, pos.y, pos.z);

        /*if(IsKeyPressed(KEY_ENTER)) {
            for(int i = 0; i < engine.phys.sysEntities.nEntries; i++) {
                PhysicsSystemEntity *ent = (PhysicsSystemEntity*)engine.phys.sysEntities.entries[i].val.ptr;
                Vector3 diff = Vector3Subtract(ent->body->pos, player.transform->pos);
                diff = Vector3Scale(diff, 4);
                diff.y = 50;
                ent->body->vel = diff;
            }
        }*/


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

    //cleanup(&engine);

    CloseWindow();
}