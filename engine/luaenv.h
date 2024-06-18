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

#include "../moonnuklear/internal.h"
#include "./ecs.h"
#include "./engine.h"
#include "./logger.h"
#include "./physcoll.h"

typedef struct LuaEnvMessageData {
    int registerValKey;
} LuaEnvMessageData;

void luaEnvSetEngine(Engine *eng);
lua_State *luaEnvCreate(struct nk_context *nk);
uint8_t luaEnvLoad(lua_State *L, const char *scriptFile, char *scriptName);

void luaEnvUpdate(lua_State *L);

EngineStatus engine_createScriptFromFile(Engine *engine, lua_State *L,
                                         ECSEntityID ent,
                                         const char *scriptFile);
EngineCompScript *engine_getScript(Engine *engine, ECSEntityID ent);