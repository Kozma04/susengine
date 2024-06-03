#pragma once

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>
#include <raymath.h>

#include "./ecs.h"
#include "./engine.h"
#include "./logger.h"
#include "./physcoll.h"

void luaEnvSetEngine(Engine *eng);
void luaEnvSetupBindings(lua_State *L);
