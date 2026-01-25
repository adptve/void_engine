# void_services - Service lifecycle management
void_add_module(NAME void_services
    SOURCES
        service.cpp
        event_bus.cpp
        services.cpp
    DEPENDENCIES
        void_core
        void_event
)
