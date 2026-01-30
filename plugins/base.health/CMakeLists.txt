# base.health Plugin - Core health/damage system
# Provides: Health, DamageReceiver, Dead components
#           HealthRegenSystem, DeathSystem
#           DamageEvent handling

add_void_plugin(
    NAME base.health
    SOURCES
        health_plugin.cpp
        health_plugin.hpp
)
