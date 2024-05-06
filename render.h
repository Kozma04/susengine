#pragma once


#include <stdint.h>
#include <stdlib.h>

#include "raylib.h"
#include "rlgl.h"

#include "ecs.h"
#include "engine.h"
#include "logger.h"

#include "dsa.h"
#include "mathutils.h"


// Manage (depth sorting, frustum culling) and draw the engine render list
void render_drawScene(Engine *const engine);