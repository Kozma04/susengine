#define USE_VSYNC

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <raylib.h>
#include <raymath.h>

#include "engine/ecs.h"
#include "engine/engine.h"
#include "engine/logger.h"
#include "engine/luaenv.h"

static void *luaAlloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    if (nsize == 0) {
        free(ptr);
        return NULL;
    } else
        return realloc(ptr, nsize);
}

int main(void) {
    char buff[256];
    int error;
    lua_State *L = lua_newstate(luaAlloc, NULL); /* opens Lua */
    luaL_openlibs(L);

    Engine *engine = malloc(sizeof(Engine));
    engine_init(engine);
    luaEnvSetEngine(engine);
    luaEnvSetupBindings(L);

    ECSEntityID entId;
    ecs_registerEntity(&engine->ecs, &entId, "testEntity");
    // engine_createInfo(engine, entId, 0x00ff);
    logMsg(LOG_LVL_INFO, "registered test entity id %d", entId);

    luaL_loadstring(
        L,
        "function dump(tbl) io.write(\"{\") for "
        "k, v in pairs(tbl) do "
        "io.write(k .. \" = \") if type(v) == \"table\" then dump(v) "
        "else io.write(tostring(v)) end io.write(\", \") end io.write(\"}\\n\")"
        "end");
    lua_pcall(L, 0, 0, 0);

    while (fgets(buff, sizeof(buff), stdin) != NULL) {
        error = luaL_loadbuffer(L, buff, strlen(buff), "line") ||
                lua_pcall(L, 0, 0, 0);
        if (error) {
            fprintf(stderr, "%s\n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    lua_close(L);
    return 0;
}