# void_ir - Declarative Intermediate Representation and Patch System
# Provides atomic transactions, namespace isolation, validation, and snapshots
void_add_module(NAME void_ir
    SOURCES
        ir.cpp
        value.cpp
        patch.cpp
        bus.cpp
        batch.cpp
        namespace.cpp
        transaction.cpp
        validation.cpp
    DEPENDENCIES
        void_core
        void_structures
)
