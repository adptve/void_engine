# void_ir - Declarative Intermediate Representation and Patch System
# Provides atomic transactions, namespace isolation, validation, and snapshots
void_add_module(NAME void_ir
    SOURCES
        stub.cpp
        # Header-only module - all implementation in headers
    DEPENDENCIES
        void_core
        void_structures
)
