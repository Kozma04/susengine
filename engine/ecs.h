#pragma once

#define ECS_COMPONENT_TYPES          10
#define ECS_MAX_ENTITIES             256
#define ECS_MAX_COMPONENTS           1024
#define ECS_COMPONENT_DATA_SIZE      352
#define ECS_COMPONENT_CALLBACK_TYPES 8

#define ECS_INVALID_ID 0xffffffff

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "./fifo.h"
#include "./dsa.h"
#include "./logger.h"


typedef uint32_t ECSEntityID;
typedef uint32_t ECSComponentID;

typedef enum ECSStatusEnum {
    ECS_RES_OK,
    ECS_RES_INVALID_PARAMS,
    ECS_RES_ENTITY_BUFF_FULL,
    ECS_RES_COMP_BUFF_FULL,
    ECS_RES_ENTITY_NOT_FOUND,
    ECS_RES_COMP_NOT_FOUND,
    ECS_RES_COMP_DUPLICATE,
    ECS_RES_CALLBACK_NOT_FOUND
} ECSStatus;

typedef struct ECSComponent ECSComponent;
typedef void (*ECSComponentCallback)(
    uint32_t cbType,
    ECSEntityID entId,
    ECSComponentID compId,
    uint32_t compType, 
    struct ECSComponent *comp,
    void *cbUserData
);

typedef struct ECSComponent {
    // User component data
    uint8_t data[ECS_COMPONENT_DATA_SIZE];
    // Callback function ptrs.
    ECSComponentCallback callback[ECS_COMPONENT_CALLBACK_TYPES];
} ECSComponent;

typedef struct ECSEntityDesc {
    // User entity alias.
    const char *name;
    // Component indices in ecs_t comp for each type.
    // Unassigned types have ECS_INVALID_ID index.
    // This is the only place where a component's type is stored in the ECS.
    uint32_t compIndex[ECS_COMPONENT_TYPES];
} ECSEntityDesc;

typedef struct ECS {
    // Registered entities count
    size_t nActiveEnt;
    // FIFO for free entity IDs
    FIFO freeEntId;
    // free_ent_id buffer
    ECSEntityID freeEntIdBuf[ECS_MAX_ENTITIES + 1];
    // Registered entity IDs. Buffer sorted in ascending order
    ECSEntityID activeEnt[ECS_MAX_ENTITIES];
    // Description for each entity ID
    ECSEntityDesc entDesc[ECS_MAX_ENTITIES];

    // FIFO for free component IDs
    FIFO freeCompId;
    // free_comp_id buffer
    ECSComponentID freeCompIdBuf[ECS_MAX_COMPONENTS + 1];
    // Description for each component ID
    ECSComponent comp[ECS_MAX_COMPONENTS];
} ECS;


void ecs_init(ECS *ecs);
void ecs_status(const ECS *ecs, uint32_t *nUsedEntities, uint32_t *nUsedComp);

// Register entity to the ECS with optional alias string (can be null) and
// write its assigned id to id_out.
// If registration failed, ECS_INVALID_ID is written to id_out.
ECSStatus ecs_registerEntity(ECS *ecs, ECSEntityID *const idOut, const char *name);
// Unregister active entity from the ECS.
ECSStatus ecs_unregisterEntity(ECS *ecs, ECSEntityID id);
// Get active entity name as C-string address. If it is not found, it's set to 0
ECSStatus ecs_getEntityNameCstr(const ECS *ecs, ECSEntityID id, const char **out);
// ecs_getEntityNameCstr wrapper to get a string pointer directly
const char *ecs_getEntityNameCstrP(const ECS *ecs, ECSEntityID id);
// Find active entity by its alias string. If it is not found, 0 is written to out
ECSStatus ecs_findEntity(const ECS *ecs, const char *name, ECSEntityID *out);
// Check if entity exists
ECSStatus ecs_entityExists(const ECS *ecs, ECSEntityID id);
// Check if component exists
ECSStatus ecs_compExists(const ECS *ecs, ECSEntityID ent, uint32_t compType);

// Register component to entity. The callbacks in comp are ignored!
ECSStatus ecs_registerComp(ECS *ecs, ECSEntityID id, uint32_t compType,
                           const ECSComponent comp);
// Unregister component from entity
ECSStatus ecs_unregisterComp(ECS *ecs, ECSEntityID id, uint32_t compType);
// Get entity's component by its type. If the component is not found, 0 is written to comp
ECSStatus ecs_getComp(ECS *ecs, ECSEntityID id, uint32_t compType,
                      ECSComponent **comp);
// ecs_GetComp wrapper for getting the component data
ECSStatus ecs_getCompData(ECS *ecs, ECSEntityID id, uint32_t compType,
                          void **data);
// Get the ID of the component with the specified type and parent entity ID
ECSStatus ecs_getCompID(const ECS *ecs, ECSEntityID id, uint32_t compType,
                        ECSComponentID *out);

// Set callback to active component selected by its type and active entity it belongs to.
ECSStatus ecs_setCallback(ECS *ecs, ECSEntityID id, uint32_t compType,
                          uint32_t cbType, ECSComponentCallback cb);
// Execute callback selected by its type, active component type and active entity
// the component belongs to.
ECSStatus ecs_execCallback(ECS *ecs, ECSEntityID id, uint32_t compType,
                           uint32_t cbType, void *cbUserData);
// Execute callback selected by its type, on all active components of the active
// specified entity.
ECSStatus ecs_execCallbackAllComp(ECS *ecs, ECSEntityID id, uint32_t cbType,
                                  void *cbUserData);
// Execute callback selected by its type, on all active components of all active
// entities.
ECSStatus ecs_execCallbackAllEnt(ECS *ecs, uint32_t cbType, void *cbUserData);
