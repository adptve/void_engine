# void_physics - Physics simulation
#
# Provides comprehensive physics simulation:
# - Multi-backend support (Jolt, PhysX, Bullet)
# - Rigidbody dynamics with full constraint system
# - Collision detection with layers and masks
# - Scene queries (raycast, shape cast, overlap)
# - Character controller
# - Hot-reload support for physics state

void_add_module(NAME void_physics
    SOURCES
        types.cpp
        shape.cpp
        body.cpp
        world.cpp
        backend.cpp
    DEPENDENCIES
        void_core
        void_math
        void_ecs
)

# Optional: Link against actual physics backends when available
# target_compile_definitions(void_physics PRIVATE VOID_HAS_JOLT)
# target_link_libraries(void_physics PRIVATE Jolt::Jolt)

# target_compile_definitions(void_physics PRIVATE VOID_HAS_PHYSX)
# target_link_libraries(void_physics PRIVATE PhysX::PhysX)

# target_compile_definitions(void_physics PRIVATE VOID_HAS_BULLET)
# target_link_libraries(void_physics PRIVATE Bullet::Bullet)
