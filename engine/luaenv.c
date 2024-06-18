#include "./luaenv.h"
#include "ecs.h"
#include "engine.h"
#include "logger.h"
#include "physcoll.h"
#include "render.h"
#include <lua.h>

static const char *metatableVector3 = "Vector3Metatable";
static const char *metatableMatrix = "Matrix4x4Metatable";
static const char *metatableQuaternion = "QuaternionMetatable";
static const char *metatableEngineComponent = "EngineComponentMetatable";

static Engine *engine;

static void *luaAlloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    if (nsize == 0) {
        free(ptr);
        return NULL;
    } else
        return realloc(ptr, nsize);
}

static Quaternion luaGetQuaternion(lua_State *L, int tableIndex);

static inline float getTableFloat(lua_State *L, int idx, const char *name) {
    lua_pushstring(L, name);
    lua_gettable(L, idx);
    if (!lua_isnumber(L, -1)) {
        luaL_error(L, "expected number in field %s", name);
        lua_pop(L, 1);
        return 0;
    }
    float val = lua_tonumber(L, -1);
    lua_pop(L, 1);
    return val;
}

static inline int getTableInteger(lua_State *L, int idx, const char *name) {
    lua_pushstring(L, name);
    lua_gettable(L, idx);
    if (!lua_isinteger(L, -1)) {
        luaL_error(L, "expected integer in field %s", name);
        lua_pop(L, 1);
        return 0;
    }
    int val = lua_tointeger(L, -1);
    lua_pop(L, 1);
    return val;
}

static inline void *getTableLightUserData(lua_State *L, int idx,
                                          const char *name) {
    lua_pushstring(L, name);
    lua_gettable(L, idx);
    if (!lua_islightuserdata(L, -1)) {
        luaL_error(L, "expected light userdata in field %s. got %s", name,
                   lua_typename(L, lua_type(L, -1)));
        lua_pop(L, 1);
        return 0;
    }
    void *val = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return val;
}

// Vector3 implementation

static int luaPushVector3Vals(lua_State *L, Vector3 val) {
    lua_createtable(L, 0, 3);
    lua_pushnumber(L, val.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, val.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, val.z);
    lua_setfield(L, -2, "z");
    luaL_getmetatable(L, metatableVector3);
    lua_setmetatable(L, -2);
    return 1;
}

static Vector3 luaGetVector3(lua_State *L, int tableIndex) {
    if (!lua_istable(L, tableIndex)) {
        luaL_error(L, "expected table at index %d", tableIndex);
        return (Vector3){0, 0, 0};
    }
    return (Vector3){getTableFloat(L, tableIndex - 1, "x"),
                     getTableFloat(L, tableIndex - 1, "y"),
                     getTableFloat(L, tableIndex - 1, "z")};
}

static int luaFuncVector3Create(lua_State *L) {
    if (lua_gettop(L) == 0)
        return luaPushVector3Vals(L, (Vector3){0, 0, 0});
    else if (lua_gettop(L) == 1)
        return luaPushVector3Vals(L, luaGetVector3(L, -1));
    else if (lua_gettop(L) == 3) {
        return luaPushVector3Vals(L, (Vector3){lua_tonumber(L, -3),
                                               lua_tonumber(L, -2),
                                               lua_tonumber(L, -1)});
    } else
        luaL_error(L, "invalid parameters count");
    return 1;
}

static int luaFuncVector3ToString(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");
    Vector3 vec = luaGetVector3(L, -1);
    lua_pushfstring(L, "[%f, %f, %f]", vec.x, vec.y, vec.z);
    return 1;
}

static int luaFuncVector3Add(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");

    Vector3 left = luaGetVector3(L, -2);
    Vector3 right = luaGetVector3(L, -1);
    luaPushVector3Vals(L, Vector3Add(left, right));
    return 1;
}

static int luaFuncVector3Negate(lua_State *L) {
    printf("VECTOR3 NEGATE\n");
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "input must be table");

    Vector3 vec = luaGetVector3(L, -1);
    luaPushVector3Vals(L, Vector3Negate(vec));
    printf("VETOR3 NEGATE OK\n");
    return 1;
}

static int luaFuncVector3Length(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "input must be table");
    lua_pushnumber(L, Vector3Length(luaGetVector3(L, -1)));
    return 1;
}

static int luaFuncVector3Normalize(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "input must be table");
    luaPushVector3Vals(L, Vector3Normalize(luaGetVector3(L, -1)));
    return 1;
}

static int luaFuncVector3Subtract(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");

    Vector3 left = luaGetVector3(L, -2);
    Vector3 right = luaGetVector3(L, -1);
    luaPushVector3Vals(L, Vector3Subtract(left, right));
    return 1;
}

static int luaFuncVector3Multiply(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1) && !lua_isnumber(L, -1))
        return luaL_error(L, "rvalue must be table or number");

    Vector3 left = luaGetVector3(L, -2);
    if (lua_istable(L, -1)) {
        Vector3 right = luaGetVector3(L, -1);
        luaPushVector3Vals(L, Vector3Multiply(left, right));
    } else
        luaPushVector3Vals(L, Vector3Scale(left, lua_tonumber(L, -1)));
    return 1;
}

static int luaFuncVector3Dot(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");

    Vector3 left = luaGetVector3(L, -2);
    Vector3 right = luaGetVector3(L, -1);
    lua_pushnumber(L, Vector3DotProduct(left, right));
    return 1;
}

static int luaFuncVector3Cross(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");

    Vector3 left = luaGetVector3(L, -2);
    Vector3 right = luaGetVector3(L, -1);
    luaPushVector3Vals(L, Vector3CrossProduct(left, right));
    return 1;
}

static int luaFuncVector3RotateByAxisAngle(lua_State *L) {
    if (lua_gettop(L) != 3)
        return luaL_error(L, "3 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -3) || !lua_istable(L, -2) || !lua_isnumber(L, -1))
        return luaL_error(L, "expected table, table, number");

    Vector3 vec = luaGetVector3(L, -3);
    Vector3 axis = luaGetVector3(L, -2);
    float angle = lua_tonumber(L, -1);
    luaPushVector3Vals(L, Vector3RotateByAxisAngle(vec, axis, angle));
    return 1;
}

static int luaFuncVector3RotateByQuaternion(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_istable(L, -1))
        return luaL_error(L, "expected table, table");

    Vector3 vec = luaGetVector3(L, -2);
    Quaternion quat = luaGetQuaternion(L, -1);
    luaPushVector3Vals(L, Vector3RotateByQuaternion(vec, quat));
    return 1;
}

static void luaOpenVector3(lua_State *L) {
    lua_pushcfunction(L, luaFuncVector3Create);
    lua_setglobal(L, "Vector3Create");
    lua_pushcfunction(L, luaFuncVector3Add);
    lua_setglobal(L, "Vector3Add");
    lua_pushcfunction(L, luaFuncVector3Subtract);
    lua_setglobal(L, "Vector3Subtract");
    lua_pushcfunction(L, luaFuncVector3Multiply);
    lua_setglobal(L, "Vector3Multiply");
    lua_pushcfunction(L, luaFuncVector3Negate);
    lua_setglobal(L, "Vector3Negate");
    lua_pushcfunction(L, luaFuncVector3Dot);
    lua_setglobal(L, "Vector3Dot");
    lua_pushcfunction(L, luaFuncVector3Length);
    lua_setglobal(L, "Vector3Length");
    lua_pushcfunction(L, luaFuncVector3Normalize);
    lua_setglobal(L, "Vector3Normalize");
    lua_pushcfunction(L, luaFuncVector3Cross);
    lua_setglobal(L, "Vector3CrossProduct");
    lua_pushcfunction(L, luaFuncVector3Dot);
    lua_setglobal(L, "Vector3DotProduct");
    lua_pushcfunction(L, luaFuncVector3RotateByAxisAngle);
    lua_setglobal(L, "Vector3RotateByAxisAngle");
    lua_pushcfunction(L, luaFuncVector3RotateByQuaternion);
    lua_setglobal(L, "Vector3RotateByQuaternion");

    luaL_newmetatable(L, metatableVector3);
    lua_pushcfunction(L, luaFuncVector3ToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, luaFuncVector3Add);
    lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, luaFuncVector3Subtract);
    lua_setfield(L, -2, "__sub");
    lua_pushcfunction(L, luaFuncVector3Multiply);
    lua_setfield(L, -2, "__mul");
    lua_pushcfunction(L, luaFuncVector3Negate);
    lua_setfield(L, -2, "__unm");
}

// Matrix implementation

static int luaPushMatrixVals(lua_State *L, Matrix val) {
    lua_createtable(L, 0, 16);
    lua_pushnumber(L, val.m0);
    lua_setfield(L, -2, "m0");
    lua_pushnumber(L, val.m1);
    lua_setfield(L, -2, "m1");
    lua_pushnumber(L, val.m2);
    lua_setfield(L, -2, "m2");
    lua_pushnumber(L, val.m3);
    lua_setfield(L, -2, "m3");
    lua_pushnumber(L, val.m4);
    lua_setfield(L, -2, "m4");
    lua_pushnumber(L, val.m5);
    lua_setfield(L, -2, "m5");
    lua_pushnumber(L, val.m6);
    lua_setfield(L, -2, "m6");
    lua_pushnumber(L, val.m7);
    lua_setfield(L, -2, "m7");
    lua_pushnumber(L, val.m8);
    lua_setfield(L, -2, "m8");
    lua_pushnumber(L, val.m9);
    lua_setfield(L, -2, "m9");
    lua_pushnumber(L, val.m10);
    lua_setfield(L, -2, "m10");
    lua_pushnumber(L, val.m11);
    lua_setfield(L, -2, "m11");
    lua_pushnumber(L, val.m12);
    lua_setfield(L, -2, "m12");
    lua_pushnumber(L, val.m13);
    lua_setfield(L, -2, "m13");
    lua_pushnumber(L, val.m14);
    lua_setfield(L, -2, "m14");
    lua_pushnumber(L, val.m15);
    lua_setfield(L, -2, "m15");

    luaL_getmetatable(L, metatableMatrix);
    lua_setmetatable(L, -2);
    return 1;
}

static Matrix luaGetMatrix(lua_State *L, int tableIndex) {
    Matrix mat;
    if (!lua_istable(L, tableIndex)) {
        luaL_error(L, "expected table at index %d", tableIndex);
        memset(&mat, 0, sizeof(Matrix));
        return mat;
    }
    mat.m0 = getTableFloat(L, tableIndex - 1, "m0");
    mat.m1 = getTableFloat(L, tableIndex - 1, "m1");
    mat.m2 = getTableFloat(L, tableIndex - 1, "m2");
    mat.m3 = getTableFloat(L, tableIndex - 1, "m3");
    mat.m4 = getTableFloat(L, tableIndex - 1, "m4");
    mat.m5 = getTableFloat(L, tableIndex - 1, "m5");
    mat.m6 = getTableFloat(L, tableIndex - 1, "m6");
    mat.m7 = getTableFloat(L, tableIndex - 1, "m7");
    mat.m8 = getTableFloat(L, tableIndex - 1, "m8");
    mat.m9 = getTableFloat(L, tableIndex - 1, "m9");
    mat.m10 = getTableFloat(L, tableIndex - 1, "m10");
    mat.m11 = getTableFloat(L, tableIndex - 1, "m11");
    mat.m12 = getTableFloat(L, tableIndex - 1, "m12");
    mat.m13 = getTableFloat(L, tableIndex - 1, "m13");
    mat.m14 = getTableFloat(L, tableIndex - 1, "m14");
    mat.m15 = getTableFloat(L, tableIndex - 1, "m15");
    return mat;
}

static int luaFuncMatrixClone(lua_State *L) {
    if (lua_gettop(L) != 1)
        luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");
    return luaPushMatrixVals(L, luaGetMatrix(L, -1));
}

static int luaFuncMatrixToString(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");
    Matrix mat = luaGetMatrix(L, -1);
    lua_pushfstring(
        L,
        "[\n\t%f, %f, %f, %f,\n\t%f, %f, %f, %f,\n\t%f, %f, %f, %f,\n\t"
        "%f, %f, %f, %f\n]",
        mat.m0, mat.m4, mat.m8, mat.m12, mat.m1, mat.m5, mat.m9, mat.m13,
        mat.m2, mat.m6, mat.m10, mat.m14, mat.m3, mat.m7, mat.m11, mat.m15);
    return 1;
}

static int luaFuncMatrixZero(lua_State *L) {
    Matrix mat;
    memset(&mat, 0, sizeof(mat));
    return luaPushMatrixVals(L, mat);
}

static int luaFuncMatrixIdentity(lua_State *L) {
    if (lua_gettop(L) != 0)
        return luaL_error(L, "0 parameters expected, got %d", lua_gettop(L));
    return luaPushMatrixVals(L, MatrixIdentity());
}

static int luaFuncMatrixTranspose(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");
    return luaPushMatrixVals(L, MatrixTranspose(luaGetMatrix(L, -1)));
}

static int luaFuncMatrixDeterminant(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");
    lua_pushnumber(L, MatrixDeterminant(luaGetMatrix(L, -1)));
    return 1;
}

static int luaFuncMatrixInvert(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");
    luaPushMatrixVals(L, MatrixInvert(luaGetMatrix(L, -1)));
    return 1;
}

static int luaFuncMatrixTranslate(lua_State *L) {
    Vector3 vec;
    if (lua_gettop(L) == 3) {
        if (!lua_isnumber(L, -1) || !lua_isnumber(L, -2) ||
            !lua_isnumber(L, -3))
            return luaL_error(L, "expected all 3 parameters to be numbers");
        vec.x = lua_tonumber(L, -3);
        vec.y = lua_tonumber(L, -2);
        vec.z = lua_tonumber(L, -1);
    } else if (lua_gettop(L) == 1) {
        if (!lua_istable(L, -1))
            return luaL_error(L, "expected table");
        vec = luaGetVector3(L, -1);
    }
    return luaPushMatrixVals(L, MatrixTranslate(vec.x, vec.y, vec.z));
}

static int luaFuncMatrixScale(lua_State *L) {
    Vector3 vec;
    if (lua_gettop(L) == 3) {
        if (!lua_isnumber(L, -1) || !lua_isnumber(L, -2) ||
            !lua_isnumber(L, -3))
            return luaL_error(L, "expected all 3 parameters to be numbers");
        vec.x = lua_tonumber(L, -3);
        vec.y = lua_tonumber(L, -2);
        vec.z = lua_tonumber(L, -1);
    } else if (lua_gettop(L) == 1) {
        if (!lua_istable(L, -1))
            return luaL_error(L, "expected table");
        vec = luaGetVector3(L, -1);
    }
    return luaPushMatrixVals(L, MatrixScale(vec.x, vec.y, vec.z));
}

static int luaFuncMatrixRotate(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_isnumber(L, -1))
        return luaL_error(L, "expected table and number");
    Matrix mat = MatrixRotate(luaGetVector3(L, -2), lua_tonumber(L, -1));
    return luaPushMatrixVals(L, mat);
}

static int luaFuncMatrixMultiply(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");
    Matrix res = MatrixMultiply(luaGetMatrix(L, -2), luaGetMatrix(L, -1));
    return luaPushMatrixVals(L, res);
}

static int luaFuncMatrixAdd(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");
    Matrix res = MatrixAdd(luaGetMatrix(L, -2), luaGetMatrix(L, -1));
    return luaPushMatrixVals(L, res);
}

static int luaFuncMatrixSubtract(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");
    Matrix res = MatrixSubtract(luaGetMatrix(L, -2), luaGetMatrix(L, -1));
    return luaPushMatrixVals(L, res);
}

static int luaFuncVector3Transform(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_istable(L, -1))
        return luaL_error(L, "expected 2 tables (vector3, matrix)");
    Vector3 res = Vector3Transform(luaGetVector3(L, -2), luaGetMatrix(L, -1));
    return luaPushVector3Vals(L, res);
}

static void luaOpenMatrix(lua_State *L) {
    lua_pushcfunction(L, luaFuncMatrixClone);
    lua_setglobal(L, "MatrixClone");
    lua_pushcfunction(L, luaFuncMatrixAdd);
    lua_setglobal(L, "MatrixAdd");
    lua_pushcfunction(L, luaFuncMatrixSubtract);
    lua_setglobal(L, "MatrixSubtract");
    lua_pushcfunction(L, luaFuncMatrixIdentity);
    lua_setglobal(L, "MatrixIdentity");
    lua_pushcfunction(L, luaFuncMatrixZero);
    lua_setglobal(L, "MatrixZero");
    lua_pushcfunction(L, luaFuncMatrixDeterminant);
    lua_setglobal(L, "MatrixDeterminant");
    lua_pushcfunction(L, luaFuncMatrixInvert);
    lua_setglobal(L, "MatrixInvert");
    lua_pushcfunction(L, luaFuncMatrixTranspose);
    lua_setglobal(L, "MatrixTranspose");
    lua_pushcfunction(L, luaFuncMatrixTranslate);
    lua_setglobal(L, "MatrixTranslate");
    lua_pushcfunction(L, luaFuncMatrixScale);
    lua_setglobal(L, "MatrixScale");
    lua_pushcfunction(L, luaFuncMatrixRotate);
    lua_setglobal(L, "MatrixRotate");
    lua_pushcfunction(L, luaFuncMatrixMultiply);
    lua_setglobal(L, "MatrixMultiply");
    lua_pushcfunction(L, luaFuncVector3Transform);
    lua_setglobal(L, "Vector3Transform");

    luaL_newmetatable(L, metatableMatrix);
    lua_pushcfunction(L, luaFuncMatrixToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, luaFuncMatrixAdd);
    lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, luaFuncMatrixSubtract);
    lua_setfield(L, -2, "__sub");
    lua_pushcfunction(L, luaFuncMatrixMultiply);
    lua_setfield(L, -2, "__mul");
}

// Quaternion implementation

static int luaPushQuaternionVals(lua_State *L, Quaternion val) {
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, val.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, val.y);
    lua_setfield(L, -2, "y");
    lua_pushnumber(L, val.z);
    lua_setfield(L, -2, "z");
    lua_pushnumber(L, val.w);
    lua_setfield(L, -2, "w");

    luaL_getmetatable(L, metatableQuaternion);
    lua_setmetatable(L, -2);
    return 1;
}

static Quaternion luaGetQuaternion(lua_State *L, int tableIndex) {
    Quaternion quat;
    if (!lua_istable(L, tableIndex)) {
        luaL_error(L, "expected table at index %d", tableIndex);
        memset(&quat, 0, sizeof(quat));
        return quat;
    }
    quat.x = getTableFloat(L, tableIndex - 1, "x");
    quat.y = getTableFloat(L, tableIndex - 1, "y");
    quat.z = getTableFloat(L, tableIndex - 1, "z");
    quat.w = getTableFloat(L, tableIndex - 1, "w");
    return quat;
}

static int luaFuncQuaternionCreate(lua_State *L) {
    Quaternion quat;
    if (lua_gettop(L) > 1)
        luaL_error(L, "expected max. 1 parameter, got %d", lua_gettop(L));
    if (lua_gettop(L) == 1) {
        if (!lua_istable(L, -1))
            return luaL_error(L, "expected table");
        quat = luaGetQuaternion(L, -1);
    } else
        quat = QuaternionIdentity();
    return luaPushQuaternionVals(L, quat);
}

static int luaFuncQuaternionToString(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");
    Quaternion quat = luaGetQuaternion(L, -1);
    lua_pushfstring(L, "[%f, %f, %f, %f]", quat.x, quat.y, quat.z, quat.w);
    return 1;
}

static int luaFuncQuaternionMultiply(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");
    Quaternion res =
        QuaternionMultiply(luaGetQuaternion(L, -2), luaGetQuaternion(L, -1));
    return luaPushQuaternionVals(L, res);
}

static int luaFuncQuaternionAdd(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");
    Quaternion res =
        QuaternionAdd(luaGetQuaternion(L, -2), luaGetQuaternion(L, -1));
    return luaPushQuaternionVals(L, res);
}

static int luaFuncQuaternionSubtract(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2))
        return luaL_error(L, "lvalue must be table");
    if (!lua_istable(L, -1))
        return luaL_error(L, "rvalue must be table");
    Quaternion res =
        QuaternionSubtract(luaGetQuaternion(L, -2), luaGetQuaternion(L, -1));
    return luaPushQuaternionVals(L, res);
}

static int luaFuncQuaternionLength(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "input must be table");
    lua_pushnumber(L, QuaternionLength(luaGetQuaternion(L, -1)));
    return 1;
}

static int luaFuncQuaternionNormalize(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "input must be table");
    luaPushQuaternionVals(L, QuaternionNormalize(luaGetQuaternion(L, -1)));
    return 1;
}

static int luaFuncQuaternionInvert(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "input must be table");
    luaPushQuaternionVals(L, QuaternionInvert(luaGetQuaternion(L, -1)));
    return 1;
}

static int luaFuncQuaternionToMatrix(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "1 parameter expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "input must be table");
    luaPushMatrixVals(L, QuaternionToMatrix(luaGetQuaternion(L, -1)));
    return 1;
}

static int luaFuncQuaternionFromEuler(lua_State *L) {
    if (lua_gettop(L) != 3)
        return luaL_error(L, "3 parameters expected, got %d", lua_gettop(L));
    if (!lua_isnumber(L, -1) || !lua_isnumber(L, -2) || !lua_isnumber(L, -3))
        return luaL_error(L, "all inputs must be numbers");
    Quaternion quat = QuaternionFromEuler(
        lua_tonumber(L, -3), lua_tonumber(L, -2), lua_tonumber(L, -1));
    luaPushQuaternionVals(L, quat);
    return 1;
}

static int luaFuncQuaternionFromAxisAngle(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "2 parameters expected, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_isnumber(L, -1))
        return luaL_error(L, "expected table, number");
    Quaternion quat =
        QuaternionFromAxisAngle(luaGetVector3(L, -2), lua_tonumber(L, -1));
    luaPushQuaternionVals(L, quat);
    return 1;
}

static void luaOpenQuaternion(lua_State *L) {
    lua_pushcfunction(L, luaFuncQuaternionCreate);
    lua_setglobal(L, "QuaternionCreate");
    lua_pushcfunction(L, luaFuncQuaternionMultiply);
    lua_setglobal(L, "QuaternionMultiply");
    lua_pushcfunction(L, luaFuncQuaternionAdd);
    lua_setglobal(L, "QuaternionAdd");
    lua_pushcfunction(L, luaFuncQuaternionSubtract);
    lua_setglobal(L, "QuaternionSubtract");
    lua_pushcfunction(L, luaFuncQuaternionLength);
    lua_setglobal(L, "QuaternionLength");
    lua_pushcfunction(L, luaFuncQuaternionNormalize);
    lua_setglobal(L, "QuaternionNormalize");
    lua_pushcfunction(L, luaFuncQuaternionInvert);
    lua_setglobal(L, "QuaternionInvert");
    lua_pushcfunction(L, luaFuncQuaternionToMatrix);
    lua_setglobal(L, "QuaternionToMatrix");
    lua_pushcfunction(L, luaFuncQuaternionFromEuler);
    lua_setglobal(L, "QuaternionFromEuler");
    lua_pushcfunction(L, luaFuncQuaternionFromAxisAngle);
    lua_setglobal(L, "QuaternionFromAxisAngle");

    luaL_newmetatable(L, metatableQuaternion);
    lua_pushcfunction(L, luaFuncQuaternionToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushcfunction(L, luaFuncQuaternionAdd);
    lua_setfield(L, -2, "__add");
    lua_pushcfunction(L, luaFuncQuaternionSubtract);
    lua_setfield(L, -2, "__sub");
    lua_pushcfunction(L, luaFuncQuaternionMultiply);
    lua_setfield(L, -2, "__mul");
}

// Misc. implementation

static int luaPushBoundingBoxVals(lua_State *L, BoundingBox val) {
    lua_createtable(L, 0, 2);
    luaPushVector3Vals(L, val.min);
    lua_setfield(L, -2, "min");
    luaPushVector3Vals(L, val.max);
    lua_setfield(L, -2, "max");

    luaL_getmetatable(L, metatableQuaternion);
    lua_setmetatable(L, -2);
    return 1;
}

static BoundingBox luaGetBoundingBox(lua_State *L, int tableIndex) {
    BoundingBox bb;
    if (!lua_istable(L, tableIndex)) {
        luaL_error(L, "expected table at index %d", tableIndex);
        memset(&bb, 0, sizeof(bb));
        return bb;
    }
    lua_getfield(L, tableIndex, "min");
    bb.min = luaGetVector3(L, -1);
    lua_pop(L, 1);
    lua_getfield(L, tableIndex, "max");
    bb.max = luaGetVector3(L, -1);
    lua_pop(L, 1);
    return bb;
}

static int luaFuncBoundingBoxCreate(lua_State *L) {
    if (lua_gettop(L) == 0)
        return luaPushBoundingBoxVals(
            L, (BoundingBox){(Vector3){0, 0, 0}, (Vector3){0, 0, 0}});
    else if (lua_gettop(L) == 1)
        return luaPushBoundingBoxVals(L, luaGetBoundingBox(L, -1));
    else if (lua_gettop(L) == 2) {
        return luaPushBoundingBoxVals(
            L, (BoundingBox){luaGetVector3(L, -2), luaGetVector3(L, -1)});
    } else
        luaL_error(L, "invalid parameters count");
    return 1;
}

static void luaOpenMisc(lua_State *L) {
    lua_pushcfunction(L, luaFuncBoundingBoxCreate);
    lua_setglobal(L, "BoundingBoxCreate");
}

static void luaPushColliderContactVals(lua_State *L, ColliderContact cont) {
    lua_createtable(L, 0, 6);
    luaPushVector3Vals(L, cont.pointA);
    lua_setfield(L, -2, "pointA");
    luaPushVector3Vals(L, cont.pointB);
    lua_setfield(L, -2, "pointB");
    luaPushVector3Vals(L, cont.normal);
    lua_setfield(L, -2, "normal");
    lua_pushnumber(L, cont.depth);
    lua_setfield(L, -2, "depth");
    lua_pushinteger(L, cont.sourceId);
    lua_setfield(L, -2, "sourceId");
    lua_pushinteger(L, cont.targetId);
    lua_setfield(L, -2, "targetId");
}

// Entity Component System implementation

static int luaEngineComponentIndex(lua_State *L) {
    const char *key = lua_tostring(L, -1);
    uint8_t isTableEntry = 0;

    EngineECSCompData *dat;
    lua_pushvalue(L, -2);
    uint32_t compType = (uint32_t)getTableInteger(L, -2, "compType");
    uint32_t entityId = (uint32_t)getTableInteger(L, -2, "entityId");
    lua_pop(L, 1);

    logPushTag("lua");
    ECSStatus res =
        ecs_getCompData(&engine->ecs, entityId, compType, (void **)&dat);
    logPopTag();

    if (res != ECS_RES_OK) {
        return luaL_error(L, "can't get comp. type %u from entity id %u: %s",
                          compType, entityId, ECSStatusStr[res]);
    }

    switch (compType) {
    case ENGINE_COMP_INFO:
        if (strcmp(key, "typeMask") == 0)
            lua_pushinteger(L, dat->info.typeMask);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_RIGIDBODY:
        if (strcmp(key, "enableDynamics") == 0)
            lua_pushboolean(L, dat->rigidBody.enableDynamics);
        else if (strcmp(key, "pos") == 0)
            luaPushVector3Vals(L, dat->rigidBody.pos);
        else if (strcmp(key, "vel") == 0)
            luaPushVector3Vals(L, dat->rigidBody.vel);
        else if (strcmp(key, "accel") == 0)
            luaPushVector3Vals(L, dat->rigidBody.accel);
        else if (strcmp(key, "gravity") == 0)
            luaPushVector3Vals(L, dat->rigidBody.gravity);
        else if (strcmp(key, "angularVel") == 0)
            luaPushVector3Vals(L, dat->rigidBody.angularVel);
        else if (strcmp(key, "inverseInertia") == 0)
            luaPushVector3Vals(L, dat->rigidBody.inverseInertia);
        else if (strcmp(key, "inverseInertiaTensor") == 0)
            luaPushMatrixVals(L, dat->rigidBody.inverseInertiaTensor);
        else if (strcmp(key, "rot") == 0)
            luaPushQuaternionVals(L, dat->rigidBody.rot);
        else if (strcmp(key, "enableRot") == 0)
            luaPushVector3Vals(L, dat->rigidBody.enableRot);
        else if (strcmp(key, "mediumFriction") == 0)
            lua_pushnumber(L, dat->rigidBody.mediumFriction);
        else if (strcmp(key, "mass") == 0)
            lua_pushnumber(L, dat->rigidBody.mass);
        else if (strcmp(key, "bounce") == 0)
            lua_pushnumber(L, dat->rigidBody.bounce);
        else if (strcmp(key, "staticFriction") == 0)
            lua_pushnumber(L, dat->rigidBody.staticFriction);
        else if (strcmp(key, "dynamicFriction") == 0)
            lua_pushnumber(L, dat->rigidBody.dynamicFriction);
        else if (strcmp(key, "cog") == 0)
            luaPushVector3Vals(L, dat->rigidBody.cog);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_TRANSFORM:
        if (strcmp(key, "anchor") == 0)
            lua_pushinteger(L, dat->trans.anchor);
        else if (strcmp(key, "pos") == 0)
            luaPushVector3Vals(L, dat->trans.pos);
        else if (strcmp(key, "scale") == 0)
            luaPushVector3Vals(L, dat->trans.scale);
        else if (strcmp(key, "rot") == 0)
            luaPushQuaternionVals(L, dat->trans.rot);
        else if (strcmp(key, "localUpdate") == 0)
            lua_pushboolean(L, dat->trans.localUpdate);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_CAMERA:
        if (strcmp(key, "pos") == 0)
            luaPushVector3Vals(L, dat->cam.cam.position);
        else if (strcmp(key, "up") == 0)
            luaPushVector3Vals(L, dat->cam.cam.up);
        else if (strcmp(key, "target") == 0)
            luaPushVector3Vals(L, dat->cam.cam.target);
        else if (strcmp(key, "fovy") == 0)
            lua_pushnumber(L, dat->cam.cam.fovy);
        else if (strcmp(key, "projection") == 0)
            lua_pushinteger(L, dat->cam.cam.projection);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_MESHRENDERER:
        if (strcmp(key, "transform") == 0)
            lua_pushinteger(L, dat->meshR.transform);
        else if (strcmp(key, "visible") == 0)
            lua_pushboolean(L, dat->meshR.visible);
        else if (strcmp(key, "castShadow") == 0)
            lua_pushboolean(L, dat->meshR.castShadow);
        else if (strcmp(key, "alpha") == 0)
            lua_pushnumber(L, dat->meshR.alpha);
        else if (strcmp(key, "color") == 0)
            luaPushVector3Vals(L, dat->meshR.color);
        else if (strcmp(key, "emissive") == 0)
            luaPushVector3Vals(L, dat->meshR.emissive);
        else if (strcmp(key, "boundingBox") == 0)
            luaPushBoundingBoxVals(L, dat->meshR.boundingBox);
        else if (strcmp(key, "modelId") == 0)
            lua_pushinteger(L, dat->meshR.modelId);
        else if (strcmp(key, "shaderId") == 0)
            lua_pushinteger(L, dat->meshR.shaderId);
        else if (strcmp(key, "distanceMode") == 0)
            lua_pushinteger(L, dat->meshR.distanceMode);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_LIGHTSOURCE:
        if (strcmp(key, "transform") == 0)
            lua_pushinteger(L, dat->light.transform);
        else if (strcmp(key, "visible") == 0)
            lua_pushboolean(L, dat->light.visible);
        else if (strcmp(key, "color") == 0)
            luaPushVector3Vals(L, dat->light.color);
        else if (strcmp(key, "type") == 0) {
            switch (dat->light.type) {
            case ENGINE_LIGHTSRC_POINT:
                lua_pushstring(L, "point");
                break;
            case ENGINE_LIGHTSRC_DIRECTIONAL:
                lua_pushstring(L, "directional");
                break;
            case ENGINE_LIGHTSRC_AMBIENT:
                lua_pushstring(L, "ambient");
                break;
            default:
                lua_pushnil(L);
                break;
            }
        } else if (strcmp(key, "pos") == 0)
            luaPushVector3Vals(L, dat->light.pos);
        else if (strcmp(key, "range") == 0)
            lua_pushnumber(L, dat->light.range);
        else if (strcmp(key, "castShadow") == 0)
            lua_pushboolean(L, dat->light.castShadow);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_COLLIDER:
        if (strcmp(key, "enabled") == 0)
            lua_pushboolean(L, dat->coll.enabled);
        else if (strcmp(key, "type") == 0) {
            switch (dat->coll.type) {
            case COLLIDER_TYPE_AABB:
                lua_pushstring(L, "aabb");
                break;
            case COLLIDER_TYPE_SPHERE:
                lua_pushstring(L, "sphere");
                break;
            case COLLIDER_TYPE_CONVEX_HULL:
                lua_pushstring(L, "convexhull");
                break;
            case COLLIDER_TYPE_HEIGHTMAP:
                lua_pushstring(L, "heightmap");
                break;
            default:
                lua_pushnil(L);
                break;
            }
        } else if (strcmp(key, "contacts") == 0) {
            lua_createtable(L, dat->coll.nContacts, 0);
            for (size_t i = 0; i < dat->coll.nContacts; i++) {
                luaPushColliderContactVals(L, dat->coll.contacts[i]);
                lua_rawseti(L, -1, i + 1);
            }
        } else if (strcmp(key, "collMask") == 0)
            lua_pushinteger(L, dat->coll.collMask);
        else if (strcmp(key, "collTargetMask") == 0)
            lua_pushinteger(L, dat->coll.collTargetMask);
        else if (strcmp(key, "bounds") == 0)
            luaPushBoundingBoxVals(L, dat->coll.bounds);
        else if (strcmp(key, "localTransform") == 0)
            luaPushMatrixVals(L, dat->coll.localTransform);
        else // access to the collider shape itself is too much for now
            isTableEntry = 1;
        break;
    default:
        isTableEntry = 1;
        break;
    }

    if (isTableEntry) {
        return 0;
        lua_rawget(L, -2);
    }
    return 1;
}

static int luaEngineComponentNewIndex(lua_State *L) {
    const char *key = lua_tostring(L, -2);
    uint8_t isTableEntry = 0;

    EngineECSCompData *dat;
    lua_pushvalue(L, -3);
    uint32_t compType = (uint32_t)getTableInteger(L, -2, "compType");
    uint32_t entityId = (uint32_t)getTableInteger(L, -2, "entityId");
    lua_pop(L, 1);

    // value is at top now

    logPushTag("lua");
    ECSStatus res =
        ecs_getCompData(&engine->ecs, entityId, compType, (void **)&dat);
    logPopTag();

    if (res != ECS_RES_OK) {
        return luaL_error(L, "can't get comp. type %u from entity id %u: %s",
                          compType, entityId, ECSStatusStr[res]);
    }

    // lua_pop(L, 1);

    switch (compType) {
    case ENGINE_COMP_INFO:
        if (strcmp(key, "typeMask") == 0)
            dat->info.typeMask = lua_tointeger(L, -1);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_RIGIDBODY:
        if (strcmp(key, "enableDynamics") == 0)
            dat->rigidBody.enableDynamics = lua_toboolean(L, -1);
        else if (strcmp(key, "pos") == 0)
            dat->rigidBody.pos = luaGetVector3(L, -1);
        else if (strcmp(key, "vel") == 0)
            dat->rigidBody.vel = luaGetVector3(L, -1);
        else if (strcmp(key, "accel") == 0)
            dat->rigidBody.accel = luaGetVector3(L, -1);
        else if (strcmp(key, "gravity") == 0)
            dat->rigidBody.gravity = luaGetVector3(L, -1);
        else if (strcmp(key, "angularVel") == 0)
            dat->rigidBody.angularVel = luaGetVector3(L, -1);
        else if (strcmp(key, "inverseInertia") == 0)
            dat->rigidBody.inverseInertia = luaGetVector3(L, -1);
        else if (strcmp(key, "rot") == 0)
            dat->rigidBody.rot = luaGetQuaternion(L, -1);
        else if (strcmp(key, "enableRot") == 0)
            dat->rigidBody.enableRot = luaGetVector3(L, -1);
        else if (strcmp(key, "mediumFriction") == 0)
            dat->rigidBody.mediumFriction = lua_tonumber(L, -1);
        else if (strcmp(key, "mass") == 0)
            dat->rigidBody.mass = lua_tonumber(L, -1);
        else if (strcmp(key, "bounce") == 0)
            dat->rigidBody.bounce = lua_tonumber(L, -1);
        else if (strcmp(key, "staticFriction") == 0)
            dat->rigidBody.staticFriction = lua_tonumber(L, -1);
        else if (strcmp(key, "dynamicFriction") == 0)
            dat->rigidBody.dynamicFriction = lua_tonumber(L, -1);
        else if (strcmp(key, "cog") == 0)
            dat->rigidBody.cog = luaGetVector3(L, -1);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_TRANSFORM:
        if (strcmp(key, "anchor") == 0)
            dat->trans.anchor = lua_tointeger(L, -1);
        else if (strcmp(key, "pos") == 0)
            dat->trans.pos = luaGetVector3(L, -1);
        else if (strcmp(key, "scale") == 0)
            dat->trans.scale = luaGetVector3(L, -1);
        else if (strcmp(key, "rot") == 0)
            dat->trans.rot = luaGetQuaternion(L, -1);
        else if (strcmp(key, "localUpdate") == 0)
            dat->trans.localUpdate = lua_toboolean(L, -1);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_CAMERA:
        if (strcmp(key, "pos") == 0)
            dat->cam.cam.position = luaGetVector3(L, -1);
        else if (strcmp(key, "up") == 0)
            dat->cam.cam.up = luaGetVector3(L, -1);
        else if (strcmp(key, "target") == 0)
            dat->cam.cam.target = luaGetVector3(L, -1);
        else if (strcmp(key, "fovy") == 0)
            dat->cam.cam.fovy = lua_tonumber(L, -1);
        else if (strcmp(key, "projection") == 0)
            dat->cam.cam.projection = lua_tointeger(L, -1);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_MESHRENDERER:
        if (strcmp(key, "transform") == 0)
            dat->meshR.transform = lua_tointeger(L, -1);
        else if (strcmp(key, "visible") == 0)
            dat->meshR.visible = lua_toboolean(L, -1);
        else if (strcmp(key, "castShadow") == 0)
            dat->meshR.castShadow = lua_toboolean(L, -1);
        else if (strcmp(key, "alpha") == 0)
            dat->meshR.alpha = lua_tonumber(L, -1);
        else if (strcmp(key, "color") == 0)
            dat->meshR.color = luaGetVector3(L, -1);
        else if (strcmp(key, "emissive") == 0)
            dat->meshR.emissive = luaGetVector3(L, -1);
        else if (strcmp(key, "boundingBox") == 0)
            dat->meshR.boundingBox = luaGetBoundingBox(L, -1);
        else if (strcmp(key, "modelId") == 0)
            dat->meshR.modelId = lua_tointeger(L, -1);
        else if (strcmp(key, "shaderId") == 0)
            dat->meshR.shaderId = lua_tointeger(L, -1);
        else if (strcmp(key, "distanceMode") == 0)
            dat->meshR.distanceMode = lua_tointeger(L, -1);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_LIGHTSOURCE:
        if (strcmp(key, "transform") == 0)
            dat->light.transform = lua_tointeger(L, -1);
        else if (strcmp(key, "visible") == 0)
            dat->light.visible = lua_toboolean(L, -1);
        else if (strcmp(key, "color") == 0)
            dat->light.color = luaGetVector3(L, -1);
        else if (strcmp(key, "pos") == 0)
            dat->light.pos = luaGetVector3(L, -1);
        else if (strcmp(key, "range") == 0)
            dat->light.range = lua_tonumber(L, -1);
        else if (strcmp(key, "castShadow") == 0)
            dat->light.castShadow = lua_toboolean(L, -1);
        else
            isTableEntry = 1;
        break;
    case ENGINE_COMP_COLLIDER:
        if (strcmp(key, "enabled") == 0)
            dat->coll.enabled = lua_toboolean(L, -1);
        else if (strcmp(key, "collMask") == 0)
            dat->coll.collMask = lua_tointeger(L, -1);
        else if (strcmp(key, "collTargetMask") == 0)
            dat->coll.collTargetMask = lua_tointeger(L, -1);
        else if (strcmp(key, "bounds") == 0)
            dat->coll.bounds = luaGetBoundingBox(L, -1);
        else if (strcmp(key, "localTransform") == 0)
            dat->coll.localTransform = luaGetMatrix(L, -1);
        else // access to the collider shape itself is too much for now
            isTableEntry = 1;
        break;
    default:
        isTableEntry = 1;
        break;
    }

    if (isTableEntry)
        lua_rawset(L, -3);
    return 0;
}

static int luaCreateComponentTable(lua_State *L, ECSEntityID id,
                                   uint32_t compType) {
    static const char *compTypeStr[] = {
        "info",         "rigidbody",   "transform", "camera",
        "meshrenderer", "lightsource", "collider"};

    ECSComponent *comp;

    logPushTag("lua");
    ECSStatus res = ecs_getComp(&engine->ecs, id, compType, &comp);
    logPopTag();
    if (res != ECS_RES_OK)
        return 0;

    lua_newtable(L);
    if (compType < ENGINE_COMP_TYPE_CNT)
        lua_pushstring(L, compTypeStr[compType]);
    else
        lua_pushstring(L, "unknown");
    lua_setfield(L, -2, "compTypeStr");
    lua_pushinteger(L, compType);
    lua_setfield(L, -2, "compType");
    lua_pushinteger(L, id);
    lua_setfield(L, -2, "entityId");

    luaL_getmetatable(L, metatableEngineComponent);
    lua_setmetatable(L, -2);

    return 1;
}

static int luaFuncCreateInfo(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "expected 2 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_isinteger(L, -1))
        return luaL_error(L, "expected table, integer");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, -3, "id");
    logPushTag("lua");
    engine_createInfo(engine, entityId, lua_tointeger(L, -1));
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_INFO)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "info");
    lua_pop(L, 2);
    return 0;
}

static int luaFuncCreateScriptFromFile(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "expected 2 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_isstring(L, -1))
        return luaL_error(L, "expected table, string");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, -3, "id");
    logPushTag("lua");
    engine_createScriptFromFile(engine, L, entityId, lua_tostring(L, -1));
    logPopTag();
    /*lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_SCRIPT)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "script");*/
    lua_pop(L, 2);
    return 0;
}

static int luaFuncSendMessage(lua_State *L) {
    // entity, target ID, message type, content

    if (lua_gettop(L) != 4)
        return luaL_error(L, "expected 4 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, 1) || !lua_isinteger(L, 2) || !lua_isinteger(L, 3))
        return luaL_error(L, "expected table, integer, integer, <any>");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, -5, "id");
    ECSEntityID targetId = lua_tointeger(L, 2);
    uint32_t msgType = lua_tointeger(L, 3);

    lua_getfield(L, LUA_REGISTRYINDEX, "currMsgKey");
    uint32_t msgKey = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_pushinteger(L, msgKey + 1);
    lua_setfield(L, LUA_REGISTRYINDEX, "currMsgKey");

    lua_getfield(L, LUA_REGISTRYINDEX, "messages");
    lua_pushinteger(L, msgKey);
    lua_pushvalue(L, 4);
    lua_settable(L, -3);
    lua_pop(L, 1);

    LuaEnvMessageData data = {msgKey};
    engine_entitySendMsg(engine, entityId, targetId, msgType, &data,
                         sizeof(data));

    return 0;
}

static int luaFuncBroadcastMessage(lua_State *L) {
    // entity, target mask, message type, content

    if (lua_gettop(L) != 4)
        return luaL_error(L, "expected 4 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, 1) || !lua_isinteger(L, 2) || !lua_isinteger(L, 3))
        return luaL_error(L, "expected table, integer, integer, <any>");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, -5, "id");
    ECSEntityID filterMask = lua_tointeger(L, 2);
    uint32_t msgType = lua_tointeger(L, 3);

    lua_getfield(L, LUA_REGISTRYINDEX, "currMsgKey");
    uint32_t msgKey = lua_tointeger(L, -1);
    lua_pop(L, 1);
    lua_pushinteger(L, msgKey + 1);
    lua_setfield(L, LUA_REGISTRYINDEX, "currMsgKey");

    lua_getfield(L, LUA_REGISTRYINDEX, "messages");
    lua_pushinteger(L, msgKey);
    lua_pushvalue(L, 4);
    lua_settable(L, -3);
    lua_pop(L, 1);

    LuaEnvMessageData data = {msgKey};
    engine_entityBroadcastMsg(engine, entityId, filterMask, msgType, &data,
                              sizeof(data));

    return 0;
}

static int luaFuncCreateRigidbody(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "expected 2 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_isnumber(L, -1))
        return luaL_error(L, "expected table, number");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, -3, "id");
    logPushTag("lua");
    engine_createRigidBody(engine, entityId, lua_tonumber(L, -1));
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_RIGIDBODY)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "rigidbody");
    lua_pop(L, 2);
    return 0;
}

static int luaFuncCreateTransform(lua_State *L) {
    ECSEntityID anchor = ECS_INVALID_ID;
    if (lua_gettop(L) > 2)
        return luaL_error(L, "expected max. 2 params, got %d", lua_gettop(L));
    if (lua_gettop(L) == 2) {
        if (!lua_isinteger(L, -1))
            return luaL_error(L, "expected integer");
        anchor = lua_tointeger(L, -1);
    }

    ECSEntityID entityId =
        (uint32_t)getTableInteger(L, -lua_gettop(L) - 1, "id");
    logPushTag("lua");
    engine_createTransform(engine, entityId, anchor);
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_TRANSFORM)) {
        lua_pop(L, lua_gettop(L));
        return 0;
    }
    lua_setfield(L, -2, "transform");
    lua_pop(L, lua_gettop(L));
    return 0;
}

static int luaFuncCreateCamera(lua_State *L) {
    float fov = 50;
    int projection = CAMERA_PERSPECTIVE;
    if (lua_gettop(L) > 3)
        return luaL_error(L, "expected 1-3 parameters, got %d", lua_gettop(L));
    if (lua_gettop(L) >= 2) {
        if (!lua_isnumber(L, 2))
            return luaL_error(L, "expected table, number");
        fov = lua_tonumber(L, 2);
    }
    if (lua_gettop(L) == 3) {
        if (!lua_isinteger(L, 3))
            return luaL_error(L, "expected table, number, integer");
        projection = lua_tointeger(L, 3);
    }

    ECSEntityID entityId = (uint32_t)getTableInteger(L, 1, "id");
    logPushTag("lua");
    engine_createCamera(engine, entityId, fov, projection);
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_CAMERA)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "camera");
    lua_pop(L, 2);
    return 0;
}

static int luaFuncCreateMeshrenderer(lua_State *L) {
    if (lua_gettop(L) != 3)
        return luaL_error(L, "expected 3 parameters, got %d", lua_gettop(L));
    if (!lua_isinteger(L, -2) || !lua_isinteger(L, -1))
        return luaL_error(L, "expected table, integer, integer");

    ECSEntityID anchor = lua_tointeger(L, -2);
    ECSEntityID modelId = lua_tointeger(L, -1);

    ECSEntityID entId = (uint32_t)getTableInteger(L, -4, "id");
    logPushTag("lua");
    engine_createMeshRenderer(engine, entId, anchor, modelId);
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entId, ENGINE_COMP_MESHRENDERER)) {
        lua_pop(L, 2);
        return 0;
    }
    engine_getMeshRenderer(engine, entId)->shaderId = SHADER_FORWARD_BASIC_ID;
    lua_setfield(L, -2, "meshrenderer");
    lua_pop(L, 2);
    return 0;
}

static int luaFuncCreateAmbientLight(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "expected 2 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_istable(L, -1))
        return luaL_error(L, "expected table, table");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, -3, "id");
    logPushTag("lua");
    engine_createAmbientLight(engine, entityId, luaGetVector3(L, -1));
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_LIGHTSOURCE)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "light");
    lua_pop(L, 2);
    return 0;
}

static int luaFuncCreateDirLight(lua_State *L) {
    if (lua_gettop(L) != 3)
        return luaL_error(L, "expected 3 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_istable(L, -1))
        return luaL_error(L, "expected table, table, table");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, -4, "id");
    logPushTag("lua");
    engine_createDirLight(engine, entityId, luaGetVector3(L, -2),
                          luaGetVector3(L, -1));
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_LIGHTSOURCE)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "light");
    lua_pop(L, 2);
    return 0;
}

static int luaFuncCreatePointLight(lua_State *L) {
    if (lua_gettop(L) != 5)
        return luaL_error(L, "expected 5 parameters, got %d", lua_gettop(L));

    ECSEntityID entityId = (uint32_t)getTableInteger(L, 1, "id");
    logPushTag("lua");
    engine_createPointLight(engine, entityId, lua_tointeger(L, 2),
                            luaGetVector3(L, 3), luaGetVector3(L, 4),
                            lua_tonumber(L, 5));
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_LIGHTSOURCE)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "light");
    lua_pop(L, 2);
    return 0;
}

static int luaFuncCreateSphereCollider(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "expected 2 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_isnumber(L, -1))
        return luaL_error(L, "expected table, number");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, 1, "id");
    logPushTag("lua");
    engine_createSphereCollider(engine, entityId, lua_tonumber(L, 2));
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_COLLIDER)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "collider");
    lua_pop(L, 2);
    return 0;
}

static int luaFuncCreateMeshCollider(lua_State *L) {
    if (lua_gettop(L) != 2)
        return luaL_error(L, "expected 2 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, -2) || !lua_isinteger(L, -1))
        return luaL_error(L, "expected table, integer");

    ECSEntityID entityId = (uint32_t)getTableInteger(L, 1, "id");
    logPushTag("lua");
    engine_createConvexHullColliderModel(engine, entityId, lua_tointeger(L, 2));
    logPopTag();
    lua_pushvalue(L, 1);
    if (!luaCreateComponentTable(L, entityId, ENGINE_COMP_COLLIDER)) {
        lua_pop(L, 2);
        return 0;
    }
    lua_setfield(L, -2, "collider");
    lua_pop(L, 2);
    return 0;
}

static int luaCreateEntityTable(lua_State *L, ECSEntityID id) {
    ECSComponent *comp;

    ECSStatus res = ecs_entityExists(&engine->ecs, id);
    if (res != ECS_RES_OK)
        return luaL_error(L, "entity id %d not found", id);

    lua_newtable(L);
    lua_pushinteger(L, id);
    lua_setfield(L, -2, "id");
    lua_pushstring(L, ecs_getEntityNameCstrP(&engine->ecs, id));
    lua_setfield(L, -2, "name");
    if (ecs_compExists(&engine->ecs, id, ENGINE_COMP_INFO) == ECS_RES_OK)
        luaCreateComponentTable(L, id, ENGINE_COMP_INFO);
    else
        lua_pushnil(L);
    lua_setfield(L, -2, "info");
    if (ecs_compExists(&engine->ecs, id, ENGINE_COMP_RIGIDBODY) == ECS_RES_OK)
        luaCreateComponentTable(L, id, ENGINE_COMP_RIGIDBODY);
    else
        lua_pushnil(L);
    lua_setfield(L, -2, "rigidbody");
    if (ecs_compExists(&engine->ecs, id, ENGINE_COMP_TRANSFORM) == ECS_RES_OK)
        luaCreateComponentTable(L, id, ENGINE_COMP_TRANSFORM);
    else
        lua_pushnil(L);
    lua_setfield(L, -2, "transform");
    if (ecs_compExists(&engine->ecs, id, ENGINE_COMP_CAMERA) == ECS_RES_OK)
        luaCreateComponentTable(L, id, ENGINE_COMP_CAMERA);
    else
        lua_pushnil(L);
    lua_setfield(L, -2, "camera");
    if (ecs_compExists(&engine->ecs, id, ENGINE_COMP_MESHRENDERER) ==
        ECS_RES_OK)
        luaCreateComponentTable(L, id, ENGINE_COMP_MESHRENDERER);
    else
        lua_pushnil(L);
    lua_setfield(L, -2, "meshrenderer");
    if (ecs_compExists(&engine->ecs, id, ENGINE_COMP_LIGHTSOURCE) == ECS_RES_OK)
        luaCreateComponentTable(L, id, ENGINE_COMP_LIGHTSOURCE);
    else
        lua_pushnil(L);
    lua_setfield(L, -2, "light");
    if (ecs_compExists(&engine->ecs, id, ENGINE_COMP_COLLIDER) == ECS_RES_OK)
        luaCreateComponentTable(L, id, ENGINE_COMP_COLLIDER);
    else
        lua_pushnil(L);
    lua_setfield(L, -2, "collider");

    lua_pushcfunction(L, luaFuncCreateInfo);
    lua_setfield(L, -2, "createInfo");
    lua_pushcfunction(L, luaFuncCreateRigidbody);
    lua_setfield(L, -2, "createRigidbody");
    lua_pushcfunction(L, luaFuncCreateTransform);
    lua_setfield(L, -2, "createTransform");
    lua_pushcfunction(L, luaFuncCreateCamera);
    lua_setfield(L, -2, "createCamera");
    lua_pushcfunction(L, luaFuncCreateMeshrenderer);
    lua_setfield(L, -2, "createMeshrenderer");
    lua_pushcfunction(L, luaFuncCreateAmbientLight);
    lua_setfield(L, -2, "createAmbientLight");
    lua_pushcfunction(L, luaFuncCreateDirLight);
    lua_setfield(L, -2, "createDirLight");
    lua_pushcfunction(L, luaFuncCreatePointLight);
    lua_setfield(L, -2, "createPointLight");
    lua_pushcfunction(L, luaFuncCreateSphereCollider);
    lua_setfield(L, -2, "createSphereCollider");
    lua_pushcfunction(L, luaFuncCreateMeshCollider);
    lua_setfield(L, -2, "createMeshCollider");
    lua_pushcfunction(L, luaFuncCreateScriptFromFile);
    lua_setfield(L, -2, "createScriptFromFile");
    lua_pushcfunction(L, luaFuncSendMessage);
    lua_setfield(L, -2, "sendMessage");
    lua_pushcfunction(L, luaFuncBroadcastMessage);
    lua_setfield(L, -2, "broadcastMessage");

    return 1;
}

static int luaFuncGetEntity(lua_State *L) {
    ECSEntityID id;
    ECSStatus res;

    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_isnumber(L, -1) && !lua_isstring(L, -1))
        return luaL_error(L, "expected number or string");

    if (lua_isnumber(L, -1))
        id = lua_tointeger(L, -1);
    else {
        logPushTag("lua");
        res = ecs_findEntity(&engine->ecs, lua_tostring(L, -1), &id);
        logPopTag();
        if (res != ECS_RES_OK)
            return luaL_error(L, "entity '%s' not found", lua_tostring(L, -1));
    }
    lua_pop(L, 1);
    return luaCreateEntityTable(L, id);
}

static int luaFuncCreateEntity(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_isstring(L, -1))
        return luaL_error(L, "expected string");

    ECSEntityID id;
    logPushTag("lua");
    ecs_registerEntity(&engine->ecs, &id, lua_tostring(L, -1));
    logPopTag();
    return luaCreateEntityTable(L, id);
}

static int luaFuncDestroyEntity(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");

    ECSEntityID id = (uint32_t)getTableInteger(L, -2, "id");
    logPushTag("lua");
    engine_entityDestroy(engine, id);
    logPopTag();
    return 0;
}

static int luaFuncPostCreateEntity(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected table");

    ECSEntityID id = (uint32_t)getTableInteger(L, -2, "id");
    logPushTag("lua");
    engine_entityPostCreate(engine, id);
    logPopTag();
    return 0;
}

static int luaFuncSetCamera(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_istable(L, -1))
        return luaL_error(L, "expected entity table");

    ECSEntityID entId = (uint32_t)getTableInteger(L, -2, "entityId");
    ECSComponentID camId;
    logPushTag("lua");
    if (ecs_getCompID(&engine->ecs, entId, ENGINE_COMP_CAMERA, &camId) ==
        ECS_RES_OK)
        engine_render_setCamera(engine, camId);
    logPopTag();
    return 0;
}

static int luaFuncRaycast(lua_State *L) {
    // position, direction, max distance, max contacts, collision mask
    if (lua_gettop(L) != 5)
        return luaL_error(L, "expected 5 parameters, got %d", lua_gettop(L));
    if (!lua_istable(L, 1) || !lua_istable(L, 2) || !lua_isnumber(L, 3) ||
        !lua_isinteger(L, 4) || !lua_isinteger(L, 5))
        return luaL_error(L, "expected table, table, number, integer, integer");
    Ray ray;
    float maxDist;
    uint32_t maxContacts, collMask;
    size_t numContacts;
    ray.position = luaGetVector3(L, -5);
    ray.direction = luaGetVector3(L, -4);
    maxDist = lua_tonumber(L, -3);
    maxContacts = lua_tointeger(L, -2);
    collMask = lua_tointeger(L, -1);

    ColliderRayContact *crc = alloca(sizeof(*crc) * maxContacts);
    physics_raycast(&engine->phys, ray, maxDist, crc, &numContacts, maxContacts,
                    collMask);

    lua_createtable(L, numContacts, 0);
    for (int i = 0; i < numContacts; i++) {
        lua_pushinteger(L, i + 1);
        lua_createtable(L, 0, 3);
        lua_pushnumber(L, crc[i].dist);
        lua_setfield(L, -2, "distance");
        lua_pushinteger(L, crc[i].id);
        lua_setfield(L, -2, "id");
        luaPushVector3Vals(L, crc[i].normal);
        lua_setfield(L, -2, "normal");
        lua_settable(L, -3);
    }

    return 1;
}

static void luaOpenEngine(lua_State *L) {
    lua_newtable(L);
    lua_pushcfunction(L, luaFuncGetEntity);
    lua_setfield(L, -2, "getEntity");
    lua_pushcfunction(L, luaFuncCreateEntity);
    lua_setfield(L, -2, "createEntity");
    lua_pushcfunction(L, luaFuncDestroyEntity);
    lua_setfield(L, -2, "destroyEntity");
    lua_pushcfunction(L, luaFuncPostCreateEntity);
    lua_setfield(L, -2, "postCreateEntity");
    lua_pushcfunction(L, luaFuncSetCamera);
    lua_setfield(L, -2, "setCamera");
    lua_pushcfunction(L, luaFuncRaycast);
    lua_setfield(L, -2, "raycast");
    lua_setglobal(L, "engine");

    luaL_newmetatable(L, metatableEngineComponent);
    lua_pushcfunction(L, luaEngineComponentIndex);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, luaEngineComponentNewIndex);
    lua_setfield(L, -2, "__newindex");
}

// Raylib bindings

static int luaFuncIsKeyDown(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_isstring(L, -1))
        return luaL_error(L, "expected string");

    lua_pushboolean(L, IsKeyDown(lua_tostring(L, -1)[0]));
    return 1;
}

static int luaFuncIsKeyPressed(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_isstring(L, -1))
        return luaL_error(L, "expected string");

    lua_pushboolean(L, IsKeyPressed(lua_tostring(L, -1)[0]));
    return 1;
}

static int luaFuncIsKeyReleased(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_isstring(L, -1))
        return luaL_error(L, "expected string");

    lua_pushboolean(L, IsKeyReleased(lua_tostring(L, -1)[0]));
    return 1;
}

static int luaFuncGetMouseDelta(lua_State *L) {
    if (lua_gettop(L) != 0)
        return luaL_error(L, "expected 0 parameters, got %d", lua_gettop(L));

    Vector2 delta = GetMouseDelta();
    /*lua_createtable(L, 0, 2);
    lua_pushnumber(L, delta.x);
    lua_setfield(L, -2, "x");
    lua_pushnumber(L, delta.y);
    lua_setfield(L, -2, "y");*/
    luaPushVector3Vals(L, (Vector3){delta.x, delta.y, 0});
    return 1;
}

static int luaFuncIsMouseButtonPressed(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_isinteger(L, -1))
        return luaL_error(L, "expected integer");

    lua_pushboolean(L, IsMouseButtonPressed(lua_tointeger(L, -1)));
    return 1;
}

static int luaFuncIsMouseButtonDown(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_isinteger(L, -1))
        return luaL_error(L, "expected integer");
    lua_pushboolean(L, IsMouseButtonDown(lua_tointeger(L, -1)));
    return 1;
}

static int luaFuncIsMouseButtonUp(lua_State *L) {
    if (lua_gettop(L) != 1)
        return luaL_error(L, "expected 1 parameter, got %d", lua_gettop(L));
    if (!lua_isinteger(L, -1))
        return luaL_error(L, "expected integer");

    lua_pushboolean(L, IsMouseButtonUp(lua_tointeger(L, -1)));
    return 1;
}

static int luaFuncGetFrameTime(lua_State *L) {
    if (lua_gettop(L) != 0)
        return luaL_error(L, "expected 0 parameters, got %d", lua_gettop(L));

    lua_pushnumber(L, GetFrameTime());
    return 1;
}

static int luaFuncGetTime(lua_State *L) {
    if (lua_gettop(L) != 0)
        return luaL_error(L, "expected 0 parameters, got %d", lua_gettop(L));

    lua_pushnumber(L, GetTime());
    return 1;
}

static void luaOpenRaylib(lua_State *L) {
    lua_pushcfunction(L, luaFuncIsKeyDown);
    lua_setglobal(L, "IsKeyDown");
    lua_pushcfunction(L, luaFuncIsKeyPressed);
    lua_setglobal(L, "IsKeyPressed");
    lua_pushcfunction(L, luaFuncIsKeyReleased);
    lua_setglobal(L, "IsKeyReleased");
    lua_pushcfunction(L, luaFuncGetMouseDelta);
    lua_setglobal(L, "GetMouseDelta");
    lua_pushcfunction(L, luaFuncIsMouseButtonPressed);
    lua_setglobal(L, "IsMouseButtonPressed");
    lua_pushcfunction(L, luaFuncIsMouseButtonDown);
    lua_setglobal(L, "IsMouseButtonDown");
    lua_pushcfunction(L, luaFuncIsMouseButtonUp);
    lua_setglobal(L, "IsMouseButtonUp");
    lua_pushcfunction(L, luaFuncGetFrameTime);
    lua_setglobal(L, "GetFrameTime");
    lua_pushcfunction(L, luaFuncGetTime);
    lua_setglobal(L, "GetTime");
}

void luaEnvSetEngine(Engine *eng) { engine = eng; }

static void luaEnvSetupBindings(lua_State *L, struct nk_context *nkCtx) {
    if (engine == NULL)
        logMsg(LOG_LVL_FATAL, "engine is NULL");

    luaL_openlibs(L);
    luaopen_moonnuklear(L);
    luaOpenVector3(L);
    luaOpenMatrix(L);
    luaOpenQuaternion(L);
    luaOpenMisc(L);
    luaOpenEngine(L);
    luaOpenRaylib(L);

    // set global Nuklear context
    lua_getglobal(L, "nk");
    lua_getfield(L, -1, "init_from_ptr");
    lua_remove(L, -2);
    lua_pushlightuserdata(L, nkCtx);
    if (lua_pcall(L, 1, 1, NULL) != LUA_OK) {
        logMsg(LOG_LVL_FATAL, "can't initialize global Nuklear context: %s",
               lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_setglobal(L, "NKCTX");

    lua_pushinteger(L, 0);
    lua_setfield(L, LUA_REGISTRYINDEX, "currMsgKey");
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "messages");

    luaL_loadstring(
        L,
        "function dump(tbl) io.write(\"{\") for "
        "k, v in pairs(tbl) do "
        "io.write(k .. \" = \") if type(v) == \"table\" then dump(v) "
        "else io.write(tostring(v)) end io.write(\", \") end io.write(\"}\\n\")"
        "end");
    lua_pcall(L, 0, 0, 0);
}

static char *loadFileStr(const char *fname) {
    FILE *file = fopen(fname, "rb");
    char *buf;
    size_t fileSize, readSize;
    if (file == NULL) {
        logMsg(LOG_LVL_ERR, "cannot open file %s", fname);
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    if (fileSize == 1) {
        logMsg(LOG_LVL_ERR, "cannot get file size for %s", fname);
        return NULL;
    }
    rewind(file);
    buf = (char *)malloc(fileSize + 1);
    if (buf == NULL) {
        logMsg(LOG_LVL_ERR, "can't allocate buffer for file %s", fname);
        fclose(file);
        return NULL;
    }
    readSize = fread(buf, 1, fileSize, file);
    if (readSize != fileSize) {
        logMsg(LOG_LVL_ERR, "can't read file %s", fname);
        free(buf);
        fclose(file);
        return NULL;
    }
    buf[fileSize] = 0;
    return buf;
}

lua_State *luaEnvCreate(struct nk_context *nk) {
    lua_State *L = lua_newstate(luaAlloc, NULL);
    luaL_openlibs(L);
    luaEnvSetupBindings(L, nk);
    return L;
}

uint8_t luaEnvLoad(lua_State *L, const char *scriptFile, char *scriptName) {
    static uint32_t scriptNum = 0;
    char *buf;

    if (engine == NULL)
        logMsg(LOG_LVL_FATAL, "engine is NULL");

    logPushTag(__FUNCTION__);
    buf = loadFileStr(scriptFile);
    logPopTag();

    if (buf == NULL)
        return 0;

    logMsg(LOG_LVL_INFO, "loading script from %s", scriptFile);
    if (luaL_loadstring(L, buf) != LUA_OK) {
        logMsg(LOG_LVL_WARN, "Lua error: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return 0;
    }
    free(buf);

    sprintf(scriptName, "script%u", scriptNum++);

    lua_newtable(L); // _ENV
    lua_newtable(L); // metatable
    lua_getglobal(L, "_G");
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    lua_setfield(L, LUA_REGISTRYINDEX, scriptName);
    lua_getfield(L, LUA_REGISTRYINDEX, scriptName);
    lua_setupvalue(L, -2, 1);

    return 1;
}

void luaEnvUpdate(lua_State *L) {
    size_t msgCnt = 0;
    lua_getfield(L, LUA_REGISTRYINDEX, "messages");
    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        msgCnt++;
        lua_pop(L, 1); // pop value
        lua_pushinteger(L, lua_tointeger(L, -1));
        lua_pushnil(L);
        lua_settable(L, -4);
    }
    lua_pop(L, 1);
    /*if (msgCnt) {
        logPushTag("lua");
        logMsg(LOG_LVL_WARN, "deleted %u unhandled messages", msgCnt);
        logPopTag();
    }*/
}

static void engineCbScriptOnCreate(uint32_t cbType, ECSEntityID entId,
                                   ECSComponentID compId, uint32_t compType,
                                   struct ECSComponent *comp,
                                   void *cbUserData) {
    if (engine == NULL)
        logMsg(LOG_LVL_FATAL, "engine is NULL");

    EngineCallbackData *cbData = cbUserData;
    EngineCompScript *script = engine_getScript(cbData->engine, entId);
    lua_State *L = script->state;

    if (L == NULL) {
        logMsg(LOG_LVL_ERR, "lua script is null");
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, script->scriptName);
    lua_getfield(L, -1, "script");
    if (!lua_istable(L, -1)) {
        logMsg(LOG_LVL_FATAL, "'script' is not a table");
        return;
    }
    lua_getfield(L, -1, "onCreate");
    if (!lua_isnil(L, -1) && !lua_isfunction(L, -1)) {
        logMsg(LOG_LVL_FATAL, "onCreate is neither nil or function");
        return;
    }
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, entId);
        lua_pushstring(L, ecs_getEntityNameCstrP(&engine->ecs, entId));
        if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
            logMsg(LOG_LVL_ERR, "onCreate error: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    } else
        lua_pop(L, 2);
    lua_pop(L, 1);
}

static void engineCbScriptOnDestroy(uint32_t cbType, ECSEntityID entId,
                                    ECSComponentID compId, uint32_t compType,
                                    struct ECSComponent *comp,
                                    void *cbUserData) {
    if (engine == NULL)
        logMsg(LOG_LVL_FATAL, "engine is NULL");

    EngineCallbackData *cbData = cbUserData;
    EngineCompScript *script = engine_getScript(cbData->engine, entId);
    lua_State *L = script->state;

    lua_getfield(L, LUA_REGISTRYINDEX, script->scriptName);
    lua_getfield(L, -1, "script");
    if (!lua_istable(L, -1)) {
        logMsg(LOG_LVL_FATAL, "'script' is not a table");
        return;
    }
    lua_getfield(L, -1, "onDestroy");
    if (!lua_isnil(L, -1) && !lua_isfunction(L, -1)) {
        logMsg(LOG_LVL_FATAL, "onDestroy is neither nil or function");
        return;
    }
    if (lua_isfunction(L, -1)) {
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            logMsg(LOG_LVL_ERR, "onDestroy error: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    } else
        lua_pop(L, 2);
    lua_pop(L, 1);
}

static void engineCbScriptOnUpdate(uint32_t cbType, ECSEntityID entId,
                                   ECSComponentID compId, uint32_t compType,
                                   struct ECSComponent *comp,
                                   void *cbUserData) {
    if (engine == NULL)
        logMsg(LOG_LVL_FATAL, "engine is NULL");

    EngineCallbackData *cbData = cbUserData;
    EngineCompScript *script = engine_getScript(cbData->engine, entId);
    lua_State *L = script->state;

    lua_getfield(L, LUA_REGISTRYINDEX, script->scriptName);
    lua_getfield(L, -1, "script");
    if (!lua_istable(L, -1)) {
        logMsg(LOG_LVL_FATAL, "'script' is not a table");
        return;
    }
    lua_getfield(L, -1, "onUpdate");
    if (!lua_isnil(L, -1) && !lua_isfunction(L, -1)) {
        logMsg(LOG_LVL_FATAL, "onUpdate is neither nil or function");
        return;
    }
    if (lua_isfunction(L, -1)) {
        lua_pushnumber(L, cbData->update.deltaTime);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            logMsg(LOG_LVL_ERR, "onUpdate error: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    } else
        lua_pop(L, 2);
    lua_pop(L, 1);
}

static void engineCbScriptOnMessage(uint32_t cbType, ECSEntityID entId,
                                    ECSComponentID compId, uint32_t compType,
                                    struct ECSComponent *comp,
                                    void *cbUserData) {
    if (engine == NULL)
        logMsg(LOG_LVL_FATAL, "engine is NULL");

    EngineCallbackData *cbData = cbUserData;
    EngineMsg *msg = cbData->msgRecv.msg;
    int contentKey = ((LuaEnvMessageData *)msg->msgData)->registerValKey;
    EngineCompScript *script = engine_getScript(cbData->engine, entId);
    lua_State *L = script->state;

    // logPushTag("lua");
    // logMsg(LOG_LVL_DEBUG, "got message from %u", msg->srcId);
    // logPopTag();

    lua_getfield(L, LUA_REGISTRYINDEX, "messages");
    lua_getfield(L, LUA_REGISTRYINDEX, script->scriptName);
    lua_getfield(L, -1, "script");
    if (!lua_istable(L, -1)) {
        logMsg(LOG_LVL_FATAL, "'script' is not a table");
        return;
    }
    lua_getfield(L, -1, "onMessage");
    if (!lua_isnil(L, -1) && !lua_isfunction(L, -1)) {
        logMsg(LOG_LVL_FATAL, "onMessage is neither nil or function");
        return;
    }
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, msg->srcId);
        lua_pushinteger(L, msg->msgType);
        lua_pushinteger(L, contentKey);
        lua_gettable(L, -7);
        if (lua_pcall(L, 3, 0, 0) != LUA_OK) {
            logMsg(LOG_LVL_ERR, "onMessage error: %s", lua_tostring(L, -1));
            lua_pop(L, 4);
        } else
            lua_pop(L, 3);
    } else
        lua_pop(L, 4);
}

EngineStatus engine_createScriptFromFile(Engine *engine, lua_State *L,
                                         ECSEntityID ent,
                                         const char *scriptFile) {
    if (engine == NULL)
        logMsg(LOG_LVL_FATAL, "engine is NULL");

    const EngineECSCompType type = ENGINE_COMP_SCRIPT;
    ECSComponent compRaw;
    EngineCompScript *const comp =
        &(((EngineECSCompData *)&compRaw.data)->script);

    if (!luaEnvLoad(L, scriptFile, comp->scriptName))
        return ENGINE_STATUS_SCRIPT_ERROR;

    comp->state = L;
    // script chunk is now at top

    lua_getfield(L, LUA_REGISTRYINDEX, comp->scriptName);
    lua_newtable(L);
    lua_pushnil(L);
    lua_setfield(L, -2, "onCreate");
    lua_pushnil(L);
    lua_setfield(L, -2, "onDestroy");
    lua_pushnil(L);
    lua_setfield(L, -2, "onUpdate");
    lua_setfield(L, -2, "script");
    lua_pop(L, 1);

    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        logMsg(LOG_LVL_FATAL, "script load error: %s", lua_tostring(L, -1));
        lua_pop(L, 2);
        return ENGINE_STATUS_SCRIPT_ERROR;
    }

    lua_pop(L, 1);

    if (ecs_registerComp(&engine->ecs, ent, type, compRaw) != ECS_RES_OK) {
        lua_close(L);
        return ENGINE_STATUS_REGISTER_FAILED;
    }

    ecs_setCallback(&engine->ecs, ent, type, ENGINE_CB_CREATE,
                    engineCbScriptOnCreate);
    ecs_setCallback(&engine->ecs, ent, type, ENGINE_CB_DESTROY,
                    engineCbScriptOnDestroy);
    ecs_setCallback(&engine->ecs, ent, type, ENGINE_CB_UPDATE,
                    engineCbScriptOnUpdate);
    ecs_setCallback(&engine->ecs, ent, type, ENGINE_CB_MSGRECV,
                    engineCbScriptOnMessage);

    return ENGINE_STATUS_OK;
}

EngineCompScript *engine_getScript(Engine *const engine,
                                   const ECSEntityID ent) {
    EngineECSCompData *data;
    const ECSStatus res =
        ecs_getCompData(&engine->ecs, ent, ENGINE_COMP_SCRIPT, (void **)&data);
    if (res != ECS_RES_OK)
        return NULL;
    return &data->script;
}