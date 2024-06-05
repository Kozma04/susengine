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
lua_State *luaEnvLoadRun(const char *scriptFile);

EngineStatus engine_createScript(Engine *engine, ECSEntityID ent,
                                 const char *scriptContent);
EngineStatus engine_createScriptFromFile(Engine *engine, ECSEntityID ent,
                                         const char *scriptFile);
EngineCompScript *engine_getScript(Engine *engine, ECSEntityID ent);