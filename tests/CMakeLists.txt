# void_engine tests
# Uses Catch2 for unit testing

# Add Catch2's cmake module path for catch_discover_tests
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

# ============================================================================
# Phase 1: Foundation Tests
# ============================================================================
void_add_test(NAME test_core
    SOURCES
        core/test_error.cpp
        core/test_version.cpp
        core/test_id.cpp
        core/test_handle.cpp
        core/test_type_registry.cpp
        core/test_plugin.cpp
        core/test_hot_reload.cpp
    DEPENDENCIES
        void_core
)

void_add_test(NAME test_math
    SOURCES
        math/test_vec.cpp
        math/test_mat.cpp
        math/test_quat.cpp
        math/test_transform.cpp
        math/test_bounds.cpp
        math/test_intersect.cpp
    DEPENDENCIES
        void_math
)

void_add_test(NAME test_structures
    SOURCES
        structures/test_slot_map.cpp
        structures/test_sparse_set.cpp
        structures/test_bitset.cpp
        structures/test_queues.cpp
    DEPENDENCIES
        void_structures
)

void_add_test(NAME test_memory
    SOURCES
        memory/test_arena.cpp
        memory/test_pool.cpp
        memory/test_stack.cpp
        memory/test_free_list.cpp
    DEPENDENCIES
        void_memory
)

void_add_test(NAME test_event
    SOURCES
        event/test_event_bus.cpp
        event/test_channel.cpp
    DEPENDENCIES
        void_event
)

# ============================================================================
# Phase 2: Data Layer Tests
# ============================================================================
void_add_test(NAME test_ecs
    SOURCES
        ecs/test_entity.cpp
        ecs/test_component.cpp
        ecs/test_world.cpp
        ecs/test_query.cpp
    DEPENDENCIES
        void_ecs
)

void_add_test(NAME test_ir
    SOURCES
        ir/test_value.cpp
        ir/test_patch.cpp
        ir/test_transaction.cpp
        ir/test_validation.cpp
        ir/test_snapshot.cpp
        ir/test_batch.cpp
    DEPENDENCIES
        void_ir
)

# ============================================================================
# Phase 3: Rendering Tests
# ============================================================================
void_add_test(NAME test_render
    SOURCES
        render/test_resource.cpp
        render/test_mesh.cpp
        render/test_material.cpp
        render/test_camera.cpp
        render/test_spatial.cpp
    DEPENDENCIES
        void_render
)

# ============================================================================
# Asset System Tests
# ============================================================================
void_add_test(NAME test_asset
    SOURCES
        asset/test_types.cpp
        asset/test_handle.cpp
        asset/test_loader.cpp
        asset/test_storage.cpp
        asset/test_server.cpp
        asset/test_hot_reload.cpp
    DEPENDENCIES
        void_asset
)

# ============================================================================
# Shader System Tests
# ============================================================================
void_add_test(NAME test_shader
    SOURCES
        shader/test_types.cpp
        shader/test_binding.cpp
        shader/test_source.cpp
        shader/test_compiler.cpp
        shader/test_registry.cpp
    DEPENDENCIES
        void_shader
)

# ============================================================================
# UI System Tests
# ============================================================================
void_add_test(NAME test_ui
    SOURCES
        ui/test_types.cpp
        ui/test_theme.cpp
        ui/test_font.cpp
        ui/test_context.cpp
        ui/test_widgets.cpp
    DEPENDENCIES
        void_ui
)

# ============================================================================
# Package System Tests
# ============================================================================
void_add_test(NAME test_package
    SOURCES
        package/test_package.cpp
        package/test_integration.cpp
    DEPENDENCIES
        void_package
        void_core
        void_ecs
)

# ============================================================================
# Integration Tests
# ============================================================================
void_add_test(NAME test_integration
    SOURCES
        integration/test_hot_reload.cpp
        integration/test_ecs_render.cpp
    DEPENDENCIES
        void_runtime
        void_ecs
        void_render
        void_core
)
