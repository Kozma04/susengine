#pragma once


#include <stdint.h>
#include <stdlib.h>

#include "raylib.h"
#include "rlgl.h"

#include "./ecs.h"
#include "./engine.h"
#include "./logger.h"

#include "./dsa.h"
#include "./mathutils.h"


enum {
    SHADER_FORWARD_BASIC_ID,
    SHADER_SHADOWMAP_ID,
    SHADER_CUBEMAP_ID,
    SHADER_WATERPLANE_ID
};


typedef struct Renderer {
    struct {
        Array meshRendVisible; // Visible Mesh Renderer component IDs
        Array meshRendVisibleDist; // distance to each Mesh Renderer
        Camera *shadowDirCam; // shadow maps cameras
        Camera mainCam; // main render camera
    } state;

    struct {
        RenderTexture *shadowMap; // depth textures only
        uint32_t resolution; // resolution for each shadow map
        size_t nCascades; // render textures
        float size; // shadow map 0 size. map n size is (n * 2 + 1) * size
    } shadowDir;

    struct {
        size_t maxSrcPoint;
        size_t maxSrcDir;
    } light;
} Renderer;


// Initialize renderer
Renderer render_init(size_t maxSrcPoint, size_t maxSrcDir);

// Setup cascade shadow mapping for directional light
void render_setupDirShadow(
    Renderer *rend, float size, size_t nCascades, uint32_t resolution
);

// Update renderer state prior to drawing
void render_updateState(Engine *const engine, Renderer *const rend);

// Fully draw the scene
void render_drawScene(Engine *engine, Renderer *rend);