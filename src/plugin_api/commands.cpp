/// @file commands.cpp
/// @brief Implementation of state modification commands

#include <void_engine/plugin_api/commands.hpp>

#include <algorithm>
#include <cmath>
#include <random>

namespace void_plugin_api {

// =============================================================================
// AI Commands Implementation
// =============================================================================

CommandResult SetBlackboardCommand::execute(AIStateStore& ai_state, CombatStateStore&,
                                             InventoryStateStore&, const CommandContext& ctx) {
    // Ensure entity has blackboard
    auto& bb = ai_state.entity_blackboards[m_entity];
    bb.last_modified = ctx.timestamp;

    // Set value based on type
    switch (m_type) {
        case ValueType::Bool:
            bb.bool_values[m_key] = m_bool_value;
            break;
        case ValueType::Int:
            bb.int_values[m_key] = m_int_value;
            break;
        case ValueType::Float:
            bb.float_values[m_key] = m_float_value;
            break;
        case ValueType::String:
            bb.string_values[m_key] = m_string_value;
            break;
        case ValueType::Vec3:
            bb.vec3_values[m_key] = m_vec3_value;
            break;
        case ValueType::Entity:
            bb.entity_values[m_key] = m_entity_value;
            break;
    }

    return CommandResult::Success;
}

bool SetBlackboardCommand::validate(const AIStateStore&, const CombatStateStore&,
                                     const InventoryStateStore&) const {
    return m_entity.value != 0 && !m_key.empty();
}

CommandResult RequestPathCommand::execute(AIStateStore& ai_state, CombatStateStore&,
                                           InventoryStateStore&, const CommandContext& ctx) {
    auto& nav = ai_state.nav_states[m_entity];
    nav.target_position = m_destination;
    nav.path_pending = true;
    nav.path_request_time = ctx.timestamp;
    nav.has_path = false;  // Will be set when path is computed

    return CommandResult::Success;
}

bool RequestPathCommand::validate(const AIStateStore&, const CombatStateStore&,
                                   const InventoryStateStore&) const {
    return m_entity.value != 0;
}

CommandResult SetPerceptionTargetCommand::execute(AIStateStore& ai_state, CombatStateStore&,
                                                   InventoryStateStore&, const CommandContext&) {
    auto& perception = ai_state.perception_states[m_entity];
    perception.primary_target = m_target;

    return CommandResult::Success;
}

bool SetPerceptionTargetCommand::validate(const AIStateStore&, const CombatStateStore&,
                                           const InventoryStateStore&) const {
    return m_entity.value != 0;
}

// =============================================================================
// Combat Commands Implementation
// =============================================================================

CommandResult ApplyDamageCommand::execute(AIStateStore&, CombatStateStore& combat_state,
                                           InventoryStateStore&, const CommandContext& ctx) {
    auto vitals_it = combat_state.entity_vitals.find(m_target);
    if (vitals_it == combat_state.entity_vitals.end()) {
        return CommandResult::InvalidEntity;
    }

    auto& vitals = vitals_it->second;

    if (!vitals.alive) {
        return CommandResult::InvalidState;
    }

    if (vitals.invulnerable) {
        m_final_damage = 0;
        return CommandResult::Success;
    }

    // Calculate final damage
    float damage = m_damage.base_damage;

    // Apply armor reduction (unless ignored)
    if (!m_damage.ignore_armor) {
        float effective_armor = std::max(0.0f, vitals.armor - m_damage.armor_penetration);
        float reduction = effective_armor / (effective_armor + 100.0f);  // Diminishing returns
        damage *= (1.0f - reduction);
    }

    // Check for crit
    if (m_damage.can_crit) {
        auto stats_it = combat_state.combat_stats.find(m_damage.source);
        if (stats_it != combat_state.combat_stats.end()) {
            // Random crit check
            static thread_local std::mt19937 rng(std::random_device{}());
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < stats_it->second.crit_chance) {
                damage *= stats_it->second.crit_multiplier;
                m_was_crit = true;
            }
        }
    }

    // Apply resistance based on damage type
    auto stats_it = combat_state.combat_stats.find(m_target);
    if (stats_it != combat_state.combat_stats.end()) {
        auto resist_it = stats_it->second.resistances.find(m_damage.type);
        if (resist_it != stats_it->second.resistances.end()) {
            damage *= (1.0f - std::clamp(resist_it->second, 0.0f, 0.9f));
        }
    }

    // Apply to shield first
    if (vitals.current_shield > 0) {
        float shield_absorbed = std::min(vitals.current_shield, damage);
        vitals.current_shield -= shield_absorbed;
        damage -= shield_absorbed;
    }

    // Apply remaining damage to health
    m_final_damage = damage;
    vitals.current_health -= damage;
    vitals.last_damage_time = ctx.timestamp;

    // Record damage history
    auto& history = combat_state.damage_history[m_target];
    history.add_entry({
        m_damage.source,
        m_final_damage,
        m_damage.type,
        ctx.timestamp,
        m_was_crit,
        false
    });
    history.total_damage_taken += m_final_damage;

    // Record damage dealt for source
    if (m_damage.source.value != 0) {
        combat_state.damage_history[m_damage.source].total_damage_dealt += m_final_damage;
    }

    // Check for death
    if (vitals.current_health <= 0) {
        vitals.current_health = 0;
        vitals.alive = false;
        vitals.death_time = ctx.timestamp;
        m_killed = true;

        // Update kill count
        if (m_damage.source.value != 0) {
            combat_state.damage_history[m_damage.source].kills++;
        }
        history.deaths++;
    }

    return CommandResult::Success;
}

bool ApplyDamageCommand::validate(const AIStateStore&, const CombatStateStore& combat_state,
                                   const InventoryStateStore&) const {
    return m_target.value != 0 && combat_state.entity_vitals.count(m_target) > 0;
}

CommandResult ApplyStatusEffectCommand::execute(AIStateStore&, CombatStateStore& combat_state,
                                                 InventoryStateStore&, const CommandContext& ctx) {
    auto& effects = combat_state.status_effects[m_target];

    // Check if effect already exists (for stacking)
    for (auto& effect : effects) {
        if (effect.effect_name == m_effect_name) {
            // Refresh duration
            effect.remaining = std::max(effect.remaining, m_duration);
            // Add stacks (up to max)
            effect.stacks = std::min(effect.stacks + m_stacks, effect.max_stacks);
            return CommandResult::Success;
        }
    }

    // Create new effect
    ActiveEffect effect;
    effect.effect_id = combat_state.next_effect_id();
    effect.effect_name = m_effect_name;
    effect.source = m_source;
    effect.duration = m_duration;
    effect.remaining = m_duration;
    effect.stacks = m_stacks;
    effect.modifiers = m_modifiers;

    effects.push_back(std::move(effect));

    return CommandResult::Success;
}

bool ApplyStatusEffectCommand::validate(const AIStateStore&, const CombatStateStore&,
                                         const InventoryStateStore&) const {
    return m_target.value != 0 && !m_effect_name.empty();
}

CommandResult HealEntityCommand::execute(AIStateStore&, CombatStateStore& combat_state,
                                          InventoryStateStore&, const CommandContext&) {
    auto vitals_it = combat_state.entity_vitals.find(m_target);
    if (vitals_it == combat_state.entity_vitals.end()) {
        return CommandResult::InvalidEntity;
    }

    auto& vitals = vitals_it->second;

    if (!vitals.alive) {
        return CommandResult::InvalidState;
    }

    float heal_amount = m_amount;

    // Heal shield first if requested
    if (m_heal_shield && vitals.current_shield < vitals.max_shield) {
        float shield_heal = std::min(heal_amount, vitals.max_shield - vitals.current_shield);
        vitals.current_shield += shield_heal;
        heal_amount -= shield_heal;
    }

    // Heal health
    if (m_over_heal) {
        vitals.current_health += heal_amount;
    } else {
        float health_heal = std::min(heal_amount, vitals.max_health - vitals.current_health);
        vitals.current_health += health_heal;
    }

    return CommandResult::Success;
}

bool HealEntityCommand::validate(const AIStateStore&, const CombatStateStore& combat_state,
                                  const InventoryStateStore&) const {
    auto it = combat_state.entity_vitals.find(m_target);
    return m_target.value != 0 && it != combat_state.entity_vitals.end() && it->second.alive;
}

CommandResult SpawnProjectileCommand::execute(AIStateStore&, CombatStateStore& combat_state,
                                               InventoryStateStore&, const CommandContext&) {
    ProjectileState projectile;
    projectile.id = combat_state.next_projectile_id();
    projectile.source = m_source;
    projectile.target = m_target;
    projectile.position = m_position;
    projectile.direction = m_direction;
    projectile.speed = m_speed;
    projectile.damage = m_damage;
    projectile.damage_type = m_damage_type;
    projectile.lifetime = m_lifetime;
    projectile.homing = m_homing;
    projectile.penetrating = m_penetrating;
    projectile.hits_remaining = m_hits;

    // Calculate velocity
    float len = std::sqrt(m_direction.x * m_direction.x +
                          m_direction.y * m_direction.y +
                          m_direction.z * m_direction.z);
    if (len > 0.0001f) {
        projectile.velocity.x = (m_direction.x / len) * m_speed;
        projectile.velocity.y = (m_direction.y / len) * m_speed;
        projectile.velocity.z = (m_direction.z / len) * m_speed;
    }

    m_spawned_id = projectile.id;
    combat_state.active_projectiles.push_back(std::move(projectile));

    return CommandResult::Success;
}

bool SpawnProjectileCommand::validate(const AIStateStore&, const CombatStateStore&,
                                       const InventoryStateStore&) const {
    return m_damage > 0;
}

// =============================================================================
// Inventory Commands Implementation
// =============================================================================

CommandResult AddItemCommand::execute(AIStateStore&, CombatStateStore&,
                                       InventoryStateStore& inventory_state,
                                       const CommandContext& ctx) {
    // Ensure entity has inventory
    auto& inventory = inventory_state.entity_inventories[m_entity];
    if (inventory.slots.empty()) {
        // Initialize with default capacity
        inventory.slots.resize(inventory.capacity);
        for (std::uint32_t i = 0; i < inventory.capacity; ++i) {
            inventory.slots[i].index = i;
        }
    }

    // Create item instance
    ItemInstanceData item_data;
    item_data.id = inventory_state.next_item_instance_id();
    item_data.def_id = m_item_def;
    item_data.quantity = m_quantity;
    item_data.quality = m_quality;
    item_data.modifiers = m_modifiers;
    item_data.acquired_time = ctx.timestamp;

    m_created_instance = item_data.id;

    // Find slot
    std::uint32_t target_slot = 0;
    bool found_slot = false;

    if (m_has_target_slot && m_target_slot < inventory.slots.size()) {
        if (inventory.slots[m_target_slot].item.value == 0) {
            target_slot = m_target_slot;
            found_slot = true;
        }
    }

    if (!found_slot) {
        // Find first empty slot
        for (std::uint32_t i = 0; i < inventory.slots.size(); ++i) {
            if (inventory.slots[i].item.value == 0 && !inventory.slots[i].locked) {
                target_slot = i;
                found_slot = true;
                break;
            }
        }
    }

    if (!found_slot) {
        m_overflow = m_quantity;
        return CommandResult::InsufficientResources;  // No space
    }

    // Place item
    inventory.slots[target_slot].item = item_data.id;
    inventory.slots[target_slot].quantity = m_quantity;

    // Register in master registry
    inventory_state.item_instances[item_data.id] = std::move(item_data);

    return CommandResult::Success;
}

bool AddItemCommand::validate(const AIStateStore&, const CombatStateStore&,
                               const InventoryStateStore&) const {
    return m_entity.value != 0 && m_item_def.value != 0 && m_quantity > 0;
}

CommandResult RemoveItemCommand::execute(AIStateStore&, CombatStateStore&,
                                          InventoryStateStore& inventory_state,
                                          const CommandContext&) {
    auto inv_it = inventory_state.entity_inventories.find(m_entity);
    if (inv_it == inventory_state.entity_inventories.end()) {
        return CommandResult::InvalidEntity;
    }

    auto& inventory = inv_it->second;

    if (m_by_def) {
        // Remove by definition
        std::uint32_t remaining = m_quantity;
        for (auto& slot : inventory.slots) {
            if (slot.item.value != 0) {
                auto item_it = inventory_state.item_instances.find(slot.item);
                if (item_it != inventory_state.item_instances.end() &&
                    item_it->second.def_id == m_item_def) {
                    std::uint32_t remove_count = std::min(remaining, slot.quantity);
                    slot.quantity -= remove_count;
                    remaining -= remove_count;
                    m_removed += remove_count;

                    if (slot.quantity == 0) {
                        if (m_destroy) {
                            inventory_state.item_instances.erase(slot.item);
                        }
                        slot.item = ItemInstanceId{};
                    }

                    if (remaining == 0) break;
                }
            }
        }
    } else {
        // Remove by instance
        for (auto& slot : inventory.slots) {
            if (slot.item == m_item) {
                std::uint32_t remove_count = (m_quantity == 0) ? slot.quantity :
                                              std::min(m_quantity, slot.quantity);
                slot.quantity -= remove_count;
                m_removed = remove_count;

                if (slot.quantity == 0) {
                    if (m_destroy) {
                        inventory_state.item_instances.erase(slot.item);
                    }
                    slot.item = ItemInstanceId{};
                }
                break;
            }
        }
    }

    return m_removed > 0 ? CommandResult::Success : CommandResult::InvalidTarget;
}

bool RemoveItemCommand::validate(const AIStateStore&, const CombatStateStore&,
                                  const InventoryStateStore& inventory_state) const {
    return m_entity.value != 0 &&
           inventory_state.entity_inventories.count(m_entity) > 0;
}

CommandResult TransferItemCommand::execute(AIStateStore&, CombatStateStore&,
                                            InventoryStateStore& inventory_state,
                                            const CommandContext& ctx) {
    (void)ctx;  // Reserved for future use (timestamps, networking)

    // Find source inventory
    auto from_it = inventory_state.entity_inventories.find(m_from);
    if (from_it == inventory_state.entity_inventories.end()) {
        return CommandResult::InvalidEntity;
    }

    // Find destination inventory
    auto to_it = inventory_state.entity_inventories.find(m_to);
    if (to_it == inventory_state.entity_inventories.end()) {
        // Create inventory for destination
        auto& dest_inv = inventory_state.entity_inventories[m_to];
        dest_inv.slots.resize(dest_inv.capacity);
        for (std::uint32_t i = 0; i < dest_inv.capacity; ++i) {
            dest_inv.slots[i].index = i;
        }
        to_it = inventory_state.entity_inventories.find(m_to);
    }

    auto& source_inv = from_it->second;
    auto& dest_inv = to_it->second;

    // Find item in source
    ContainerSlot* source_slot = nullptr;
    for (auto& slot : source_inv.slots) {
        if (slot.item == m_item) {
            source_slot = &slot;
            break;
        }
    }

    if (!source_slot) {
        return CommandResult::InvalidTarget;
    }

    // Find empty slot in destination
    ContainerSlot* dest_slot = nullptr;
    for (auto& slot : dest_inv.slots) {
        if (slot.item.value == 0 && !slot.locked) {
            dest_slot = &slot;
            break;
        }
    }

    if (!dest_slot) {
        return CommandResult::InsufficientResources;
    }

    // Transfer
    std::uint32_t transfer_qty = (m_quantity == 0) ? source_slot->quantity :
                                  std::min(m_quantity, source_slot->quantity);

    dest_slot->item = source_slot->item;
    dest_slot->quantity = transfer_qty;
    source_slot->quantity -= transfer_qty;

    if (source_slot->quantity == 0) {
        source_slot->item = ItemInstanceId{};
    }

    return CommandResult::Success;
}

bool TransferItemCommand::validate(const AIStateStore&, const CombatStateStore&,
                                    const InventoryStateStore& inventory_state) const {
    return m_from.value != 0 && m_to.value != 0 && m_item.value != 0 &&
           inventory_state.entity_inventories.count(m_from) > 0;
}

CommandResult EquipItemCommand::execute(AIStateStore&, CombatStateStore&,
                                         InventoryStateStore& inventory_state,
                                         const CommandContext&) {
    auto inv_it = inventory_state.entity_inventories.find(m_entity);
    if (inv_it == inventory_state.entity_inventories.end()) {
        return CommandResult::InvalidEntity;
    }

    // Find item in inventory
    ContainerSlot* item_slot = nullptr;
    for (auto& slot : inv_it->second.slots) {
        if (slot.item == m_item) {
            item_slot = &slot;
            break;
        }
    }

    if (!item_slot) {
        return CommandResult::InvalidTarget;
    }

    // Ensure entity has equipment
    auto& equipment = inventory_state.equipment[m_entity];

    // Determine slot (use provided or default to "MainHand")
    std::string slot_name = m_slot.empty() ? "MainHand" : m_slot;

    // Ensure slot exists
    if (equipment.slots.find(slot_name) == equipment.slots.end()) {
        EquipmentSlotData slot_data;
        slot_data.slot_name = slot_name;
        equipment.slots[slot_name] = slot_data;
    }

    auto& equip_slot = equipment.slots[slot_name];

    // Unequip existing item if present
    if (equip_slot.equipped_item.value != 0) {
        m_unequipped = equip_slot.equipped_item;
        // Put back in inventory (find empty slot)
        for (auto& slot : inv_it->second.slots) {
            if (slot.item.value == 0 && !slot.locked) {
                slot.item = m_unequipped;
                slot.quantity = 1;
                break;
            }
        }
    }

    // Equip new item
    equip_slot.equipped_item = m_item;

    // Remove from inventory
    item_slot->item = ItemInstanceId{};
    item_slot->quantity = 0;

    return CommandResult::Success;
}

bool EquipItemCommand::validate(const AIStateStore&, const CombatStateStore&,
                                 const InventoryStateStore& inventory_state) const {
    return m_entity.value != 0 && m_item.value != 0 &&
           inventory_state.item_instances.count(m_item) > 0;
}

CommandResult StartCraftingCommand::execute(AIStateStore&, CombatStateStore&,
                                             InventoryStateStore& inventory_state,
                                             const CommandContext&) {
    auto& crafting = inventory_state.crafting_queues[m_entity];

    if (crafting.queue.size() >= crafting.max_queue_size) {
        return CommandResult::InsufficientResources;
    }

    CraftingQueueEntry entry;
    entry.recipe_id = m_recipe_id;
    entry.progress = 0;
    entry.total_time = 10.0f;  // Default - would be looked up from recipe registry
    entry.paused = false;

    crafting.queue.push_back(std::move(entry));

    return CommandResult::Success;
}

bool StartCraftingCommand::validate(const AIStateStore&, const CombatStateStore&,
                                     const InventoryStateStore&) const {
    return m_entity.value != 0 && m_recipe_id != 0;
}

// =============================================================================
// Command Processor Implementation
// =============================================================================

CommandResult CommandProcessor::execute(CommandPtr command, const CommandContext& ctx) {
    if (!command) {
        return CommandResult::Failed;
    }

    // Validate first
    if (!command->validate(m_ai_state, m_combat_state, m_inventory_state)) {
        ++m_commands_failed;
        return CommandResult::Failed;
    }

    // Execute
    auto result = command->execute(m_ai_state, m_combat_state, m_inventory_state, ctx);

    if (result == CommandResult::Success) {
        ++m_commands_executed;
    } else {
        ++m_commands_failed;
    }

    // Notify callbacks
    for (auto& callback : m_callbacks) {
        callback(*command, result);
    }

    return result;
}

void CommandProcessor::queue(CommandPtr command, const CommandContext& ctx) {
    if (command) {
        m_queue.emplace_back(std::move(command), ctx);
    }
}

std::vector<CommandResult> CommandProcessor::process_queue() {
    std::vector<CommandResult> results;
    results.reserve(m_queue.size());

    for (auto& [cmd, ctx] : m_queue) {
        results.push_back(execute(std::move(cmd), ctx));
    }

    m_queue.clear();
    return results;
}

} // namespace void_plugin_api
