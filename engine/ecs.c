#include "./ecs.h"


static inline uint8_t ecs_checkCompType(const uint32_t compType) {
    if(compType >= ECS_COMPONENT_TYPES) {
        logMsg(
            LOG_LVL_ERR, "component type id out of range: %u vs %u",
            compType, ECS_COMPONENT_TYPES
        );
        return 0;
    }
    return 1;
}

static inline uint8_t ecs_checkEntityID(const ECSEntityID id) {
    if(id == ECS_INVALID_ID) {
        logMsg(LOG_LVL_ERR, "input entity id is ECS_INVALID_ID");
        return 0;
    }
    if(id >= ECS_MAX_ENTITIES) {
        logMsg(
            LOG_LVL_ERR,
            "entity id larger than max. possible entity count: %u vs %u",
            id, ECS_MAX_ENTITIES
        );
        return 0;
    }
    return 1;
}

static inline uint8_t ecs_checkComponentID(const ECSComponentID id) {
    if(id == ECS_INVALID_ID) {
        logMsg(LOG_LVL_ERR, "input component id is ECS_INVALID_ID");
        return 0;
    }
    if(id >= ECS_MAX_COMPONENTS) {
        logMsg(
            LOG_LVL_ERR,
            "component id larger than max. possible component count: %u vs %u",
            id, ECS_MAX_COMPONENTS
        );
        return 0;
    }
    return 1;
}

static inline uint8_t ecs_checkCallbackType(const uint32_t cbType) {
    if(cbType >= ECS_COMPONENT_CALLBACK_TYPES) {
        logMsg(
            LOG_LVL_ERR, "component callback type out of range: %u vs %u",
            cbType, ECS_COMPONENT_CALLBACK_TYPES
        );
        return 0;
    }
    return 1;
}

static inline uint8_t ecs_checkEntityExists(const ECS *const ecs,
                                            const ECSEntityID id) {
    if(!binsearch_s32Inc(ecs->activeEnt, ecs->nActiveEnt, id, 0)) {
        logMsg(LOG_LVL_ERR, "entity ID %u not found", id);
        return 0;
    }
    return 1;
}


void ecs_init(ECS *const ecs) {
    ECSEntityID i;
    ecs->nActiveEnt = 0;
    fifo_init(
        &ecs->freeEntId, ecs->freeEntIdBuf, ECS_MAX_ENTITIES + 1,
        FIFO_MODE_NO_OVERRUN
    );
    fifo_init(
        &ecs->freeCompId, ecs->freeCompIdBuf, ECS_MAX_COMPONENTS + 1,
        FIFO_MODE_NO_OVERRUN
    );
    for(i = 0; i < ECS_MAX_ENTITIES; i++)
        fifo_write(&ecs->freeEntId, i);
    for(i = 0; i < ECS_MAX_COMPONENTS; i++)
        fifo_write(&ecs->freeCompId, i);
}

void ecs_status(const ECS *const ecs, uint32_t *const nUsedEntities,
                uint32_t* const nUsedComp) {
    if(nUsedEntities) *nUsedEntities = ecs->nActiveEnt;
    if(nUsedComp) *nUsedComp = fifo_av_write(&ecs->freeCompId);
}

ECSStatus ecs_registerEntity(ECS *const ecs, ECSEntityID *const id_out,
                             const char *const name) {
    ECSEntityID id = ECS_INVALID_ID;
    if(!fifo_av_read(&ecs->freeEntId)) {
        *id_out = id;
        logMsg(LOG_LVL_WARN, "entity buffer full");
        return ECS_RES_ENTITY_BUFF_FULL;
    }
    id = fifo_read(&ecs->freeEntId);
    insertsort_u32Inc(ecs->activeEnt, ecs->nActiveEnt++, id);
    ecs->entDesc[id].name = name;

    /*printf("Active entities: ");
    for(uint32_t i = 0; i < ecs->nActiveEnt; i++)
        printf("%u ", ecs->activeEnt[i]);
    printf("\n");*/

    for(uint32_t i = 0; i < ECS_COMPONENT_TYPES; i++)
        ecs->entDesc[id].compIndex[i] = ECS_INVALID_ID;
    
    *id_out = id;
    logMsg(
        LOG_LVL_INFO, "registered entity %u/%u (\"%s\")",
        id, ECS_MAX_ENTITIES - 1, name
    );
    return ECS_RES_OK;
}

ECSStatus ecs_registerComp(
    ECS *const ecs, const ECSEntityID id, const uint32_t compType,
    const ECSComponent comp
) {
    ECSEntityDesc *const desc = ecs->entDesc + id;
    if(!ecs_checkCompType(compType) || !ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    
    if(!ecs_checkEntityExists(ecs, id))
        return ECS_RES_ENTITY_NOT_FOUND;
    if(!fifo_av_read(&ecs->freeCompId)) {
        logMsg(LOG_LVL_ERR, "component buffer full");
        return ECS_RES_COMP_BUFF_FULL;
    }
    if(desc->compIndex[compType] != ECS_INVALID_ID) {
        logMsg(LOG_LVL_ERR, "duplicate component");
        return ECS_RES_COMP_DUPLICATE;
    }
    const uint32_t compId = fifo_read(&ecs->freeCompId);
    ecs->comp[compId] = comp;
    for(uint32_t i = 0; i < ECS_COMPONENT_CALLBACK_TYPES; i++)
        ecs->comp[compId].callback[i] = 0;
    desc->compIndex[compType] = compId;

    logMsg(
        LOG_LVL_INFO, "registered comp. %u/%u of type %u to entity id %u (\"%s\")",
        compId, ECS_MAX_COMPONENTS - 1, compType, id, ecs_getEntityNameCstrP(ecs, id)
    );
    return ECS_RES_OK;
}

ECSStatus ecs_unregisterComp(ECS *const ecs, const ECSEntityID id,
                              const uint32_t compType) {
    ECSEntityDesc *const desc = ecs->entDesc + id;

    if(!ecs_checkCompType(compType) || !ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    
    if(!ecs_checkEntityExists(ecs, id))
        return ECS_RES_ENTITY_NOT_FOUND;
    uint32_t *const compId = desc->compIndex + compType;
    if(!ecs_checkComponentID(*compId))
        return ECS_RES_COMP_NOT_FOUND;
    fifo_write(&ecs->freeCompId, *compId);
    *compId = ECS_INVALID_ID;

    logMsg(
        LOG_LVL_INFO, "unregistered comp. %u of type %u from entity id %u(\"%s\")",
        compId, compType, id, ecs_getEntityNameCstrP(ecs, id)
    );
    return ECS_RES_OK;
}

ECSStatus ecs_getComp(ECS *const ecs, const ECSEntityID id,
                      const uint32_t compType, ECSComponent **const comp) {
    const ECSEntityDesc *const desc = ecs->entDesc + id;
    *comp = 0;
    if(!ecs_checkCompType(compType) || !ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    
    if(!ecs_checkEntityExists(ecs, id))
        return ECS_RES_ENTITY_NOT_FOUND;
    const uint32_t *const compId = desc->compIndex + compType;
    
    if(!ecs_checkComponentID(*compId))
        return ECS_RES_COMP_NOT_FOUND;
    *comp = ecs->comp + *compId;
    return ECS_RES_OK;
}

ECSStatus ecs_getCompData(ECS *ecs, const ECSEntityID id,
                          const uint32_t compType, void **const data) {
    ECSComponent *comp;
    const ECSStatus stat = ecs_getComp(ecs, id, compType, &comp);
    if(stat != ECS_RES_OK) {
        *data = 0;
        return stat;
    }
    *data = comp->data;
    return ECS_RES_OK;
}

ECSStatus ecs_getCompID(const ECS *ecs, const ECSEntityID id,
                        const uint32_t compType, ECSComponentID *const out) {
    const ECSEntityDesc *const desc = ecs->entDesc + id;
    *out = ECS_INVALID_ID;
    if(!ecs_checkCompType(compType) || !ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    
    if(!ecs_checkEntityExists(ecs, id))
        return ECS_RES_ENTITY_NOT_FOUND;
    const uint32_t *const compId = desc->compIndex + compType;
    
    if(!ecs_checkComponentID(*compId))
        return ECS_RES_COMP_NOT_FOUND;
    *out = *compId;
    return ECS_RES_OK;
}

ECSStatus ecs_unregisterEntity(ECS *const ecs, const ECSEntityID id) {
    uint32_t targetEntPos, i, *pComp;
    ECSEntityDesc *const desc = ecs->entDesc + id;

    if(!ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    
    if(!binsearch_s32Inc(ecs->activeEnt, ecs->nActiveEnt, id, &targetEntPos)) {
        logMsg(LOG_LVL_ERR, "entity ID %u not found", id);
        return 0;
    }

    const char *const entName = ecs_getEntityNameCstrP(ecs, id);

    // Unregister its components
    for(i = 0, pComp = desc->compIndex; i < ECS_COMPONENT_TYPES; i++, pComp++) {
        if(*pComp != ECS_INVALID_ID) {
            fifo_write(&ecs->freeCompId, *pComp);
            *pComp = ECS_INVALID_ID;
        }
    }

    // Unregister entity
    fifo_write(&ecs->freeEntId, id);
    for(i = targetEntPos; i < ecs->nActiveEnt - 1; i++)
        ecs->activeEnt[i] = ecs->activeEnt[i + 1];
    ecs->nActiveEnt--;

    logMsg(
        LOG_LVL_INFO, "unregistered entity %u (\"%s\") and its components",
        id, entName
    );
    return ECS_RES_OK;
}

ECSStatus ecs_getEntityNameCstr(
    const ECS *const ecs, const ECSEntityID id, const char **out
) {
    const ECSEntityDesc *const desc = ecs->entDesc + id;
    
    if(!ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    
    if(!binsearch_s32Inc(ecs->activeEnt, ecs->nActiveEnt, id, 0)) {
        *out = 0;
        return ECS_RES_ENTITY_NOT_FOUND;
    }
    *out = desc->name;
    return ECS_RES_OK;
}

const char *ecs_getEntityNameCstrP(const ECS *const ecs, const ECSEntityID id) {
    const static char *invalidIdStr = "NULL_PTR";
    const char *name;
    if(ecs_getEntityNameCstr(ecs, id, &name) != ECS_RES_OK)
        return invalidIdStr;
    return name;
}

ECSStatus ecs_findEntity(const ECS *const ecs, const char *const nameMatch,
                         ECSEntityID *const out) {
    ECSComponent *comp;
    uint32_t entId;
    for(uint32_t i = 0; i < ecs->nActiveEnt; i++) {
        entId = ecs->activeEnt[i];
        if(strcmp(ecs->entDesc[entId].name, nameMatch) == 0) {
            *out = entId;
            return ECS_RES_OK;
        }
    }
    *out = ECS_INVALID_ID;
    logMsg(LOG_LVL_WARN, "entity ID not found for name \"%s\"", nameMatch);
    return ECS_RES_ENTITY_NOT_FOUND;
}

ECSStatus ecs_entityExists(const ECS *const ecs, const ECSEntityID id) {
    if(id == ECS_INVALID_ID)
        return ECS_RES_ENTITY_NOT_FOUND;
    if(binsearch_s32Inc(ecs->activeEnt, ecs->nActiveEnt, id, 0))
        return ECS_RES_ENTITY_NOT_FOUND;
    return ECS_RES_OK;
}

ECSStatus ecs_compExists(
    const ECS *ecs, const ECSEntityID ent, const uint32_t compType
) {
    ECSStatus res = ecs_entityExists(ecs, ent);
    if(res != ECS_RES_OK)
        return res;
    const ECSComponentID comp = ecs->entDesc[ent].compIndex[compType];
    if(comp == ECS_INVALID_ID)
        return ECS_RES_COMP_NOT_FOUND;
    return ECS_RES_OK;
}

ECSStatus ecs_setCallback(
    ECS *const ecs, const ECSEntityID id, const uint32_t compType,
    const uint32_t cbType, ECSComponentCallback cb
) {
    ECSEntityDesc* const desc = ecs->entDesc + id;
   
    if(!ecs_checkCompType(compType) || !ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    if(!ecs_checkCallbackType(cbType))
        return ECS_RES_INVALID_PARAMS;
    if(!ecs_checkEntityExists(ecs, id))
        return ECS_RES_ENTITY_NOT_FOUND;
    const uint32_t compId = desc->compIndex[compType];
    if(compId == ECS_INVALID_ID)
        return ECS_RES_COMP_NOT_FOUND;
    ecs->comp[compId].callback[cbType] = cb;

    logMsg(
        LOG_LVL_DEBUG,
        "assigned callback of type %u to comp. type %u of entity %u(\"%s\")",
        cbType, compType, id, ecs_getEntityNameCstrP(ecs, id)
    )
    return ECS_RES_OK;
}

ECSStatus ecs_execCallback(
    ECS *const ecs, const ECSEntityID id, const uint32_t compType,
    const uint32_t cbType, void *cbUserData
) {
    ECSEntityDesc *const desc = ecs->entDesc + id;
    
    if(!ecs_checkCompType(compType) || !ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    if(!ecs_checkCallbackType(cbType))
        return ECS_RES_INVALID_PARAMS;
    if(!ecs_checkEntityExists(ecs, id))
        return ECS_RES_ENTITY_NOT_FOUND;
    const uint32_t compId = desc->compIndex[compType];
    if(compId == ECS_INVALID_ID)
        return ECS_RES_COMP_NOT_FOUND;
    ECSComponentCallback cb = ecs->comp[compId].callback[cbType];
    if(cb)
        cb(cbType, id, compId, compType, ecs->comp + compId, cbUserData);
    return ECS_RES_OK;
}

ECSStatus ecs_execCallbackAllComp(
    ECS *const ecs, const ECSEntityID id, const uint32_t cbType,
    void *cbUserData
) {
    ECSEntityDesc *const desc = ecs->entDesc + id;
    uint32_t compId;
    
    if(!ecs_checkEntityID(id))
        return ECS_RES_INVALID_PARAMS;
    if(!ecs_checkCallbackType(cbType))
        return ECS_RES_INVALID_PARAMS;
    if(!ecs_checkEntityExists(ecs, id))
        return ECS_RES_ENTITY_NOT_FOUND;
    for(uint32_t compType = 0; compType < ECS_COMPONENT_TYPES; compType++) {
        compId = desc->compIndex[compType];
        if(compId == ECS_INVALID_ID)
            continue;
        ECSComponentCallback cb = ecs->comp[compId].callback[cbType];
        if(cb) {
            cb(cbType, id, compId, compType, ecs->comp + compId, cbUserData);
        }
    }
    return ECS_RES_OK;
}

ECSStatus ecs_execCallbackAllEnt(ECS *const ecs, const uint32_t cbType, 
                                 void *cbUserData) {
    ECSComponent *comp;
    uint32_t entId, compId;
    
    if(!ecs_checkCallbackType(cbType))
        return ECS_RES_INVALID_PARAMS;
    for(uint32_t i = 0; i < ecs->nActiveEnt; i++) {
        entId = ecs->activeEnt[i];
        for(uint32_t compType = 0; compType < ECS_COMPONENT_TYPES; compType++) {
            compId = ecs->entDesc[entId].compIndex[compType];
            if(compId == ECS_INVALID_ID)
                continue;
            comp = ecs->comp + compId;
            ECSComponentCallback cb = comp->callback[cbType];
            if(cb)
                cb(cbType, entId, compId, compType, ecs->comp + compId, cbUserData);
        }
    }
    return ECS_RES_OK;
}
