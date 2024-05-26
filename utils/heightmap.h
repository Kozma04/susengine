#pragma once

#include <raylib.h>
#include <raymath.h>
#include <stdint.h>

#include "../engine/ecs.h"
#include "../engine/engine.h"
#include "../engine/logger.h"
#include "../engine/mathutils.h"
#include "../engine/physcoll.h"

Mesh GenMeshHeightmapChunk(Image hmap, int px, int py, int w, int h);