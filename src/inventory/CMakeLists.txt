# void_inventory - Inventory system
void_add_module(NAME void_inventory
    SOURCES
        types.cpp
        items.cpp
        containers.cpp
        equipment.cpp
        crafting.cpp
        inventory.cpp
    DEPENDENCIES
        void_core
        void_ecs
        void_event
)
