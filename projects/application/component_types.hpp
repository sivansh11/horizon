#ifndef COMPONENT_TYPES_HPP
#define COMPONENT_TYPES_HPP

#include "core/ecs.hpp"

struct children_t {
    uint32_t size;
    core::entity_id_t first;
};

struct relationship_t {
    core::entity_id_t parent;
    core::entity_id_t next;
    core::entity_id_t prev;
};

#endif