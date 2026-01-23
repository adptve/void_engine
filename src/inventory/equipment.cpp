/// @file equipment.cpp
/// @brief Equipment system implementation for void_inventory module

#include <void_engine/inventory/equipment.hpp>

#include <algorithm>
#include <chrono>

namespace void_inventory {

// =============================================================================
// EquipmentSetRegistry Implementation
// =============================================================================

EquipmentSetRegistry::EquipmentSetRegistry() = default;
EquipmentSetRegistry::~EquipmentSetRegistry() = default;

void EquipmentSetRegistry::register_set(const std::string& name, const EquipmentSetDef& set) {
    m_sets[name] = set;
}

const EquipmentSetDef* EquipmentSetRegistry::get_set(const std::string& name) const {
    auto it = m_sets.find(name);
    return it != m_sets.end() ? &it->second : nullptr;
}

std::vector<const EquipmentSetDef*> EquipmentSetRegistry::find_sets_with_item(ItemDefId item) const {
    std::vector<const EquipmentSetDef*> result;
    for (const auto& [name, set] : m_sets) {
        if (set.contains_item(item)) {
            result.push_back(&set);
        }
    }
    return result;
}

std::vector<std::string> EquipmentSetRegistry::all_sets() const {
    std::vector<std::string> result;
    result.reserve(m_sets.size());
    for (const auto& [name, set] : m_sets) {
        result.push_back(name);
    }
    return result;
}

void EquipmentSetRegistry::clear() {
    m_sets.clear();
}

EquipmentSetDef EquipmentSetRegistry::preset_iron_set() {
    EquipmentSetDef set;
    set.name = "Iron Set";
    set.description = "A full set of iron equipment.";

    // Would normally reference registered item IDs
    // For now, items would be added after registration

    SetBonus bonus2;
    bonus2.pieces_required = 2;
    bonus2.name = "Iron Will";
    bonus2.description = "+10 Defense";
    bonus2.stat_bonuses.push_back({StatType::Defense, ModifierType::Flat, 10.0f, "Iron Set (2)"});
    set.bonuses.push_back(bonus2);

    SetBonus bonus4;
    bonus4.pieces_required = 4;
    bonus4.name = "Iron Fortress";
    bonus4.description = "+25 Defense, +5% Physical Resist";
    bonus4.stat_bonuses.push_back({StatType::Defense, ModifierType::Flat, 25.0f, "Iron Set (4)"});
    bonus4.stat_bonuses.push_back({StatType::PhysicalResist, ModifierType::Percent, 0.05f, "Iron Set (4)"});
    set.bonuses.push_back(bonus4);

    return set;
}

EquipmentSetDef EquipmentSetRegistry::preset_leather_set() {
    EquipmentSetDef set;
    set.name = "Leather Set";
    set.description = "A full set of leather armor.";

    SetBonus bonus2;
    bonus2.pieces_required = 2;
    bonus2.name = "Light Feet";
    bonus2.description = "+5% Move Speed";
    bonus2.stat_bonuses.push_back({StatType::MoveSpeed, ModifierType::Percent, 0.05f, "Leather Set (2)"});
    set.bonuses.push_back(bonus2);

    SetBonus bonus4;
    bonus4.pieces_required = 4;
    bonus4.name = "Nimble";
    bonus4.description = "+10% Move Speed, +5% Attack Speed";
    bonus4.stat_bonuses.push_back({StatType::MoveSpeed, ModifierType::Percent, 0.10f, "Leather Set (4)"});
    bonus4.stat_bonuses.push_back({StatType::AttackSpeed, ModifierType::Percent, 0.05f, "Leather Set (4)"});
    set.bonuses.push_back(bonus4);

    return set;
}

// =============================================================================
// EquipmentComponent Implementation
// =============================================================================

EquipmentComponent::EquipmentComponent() = default;

EquipmentComponent::EquipmentComponent(EntityId owner)
    : m_owner(owner) {
}

EquipmentComponent::~EquipmentComponent() = default;

EquipmentSlotId EquipmentComponent::add_slot(const EquipmentSlotDef& def) {
    EquipmentSlotId id{m_next_slot_id++};
    EquipmentSlotDef registered_def = def;
    registered_def.id = id;
    m_slot_defs[id] = registered_def;
    return id;
}

bool EquipmentComponent::remove_slot(EquipmentSlotId slot) {
    // Unequip first
    unequip(slot, nullptr);
    return m_slot_defs.erase(slot) > 0;
}

const EquipmentSlotDef* EquipmentComponent::get_slot_def(EquipmentSlotId slot) const {
    auto it = m_slot_defs.find(slot);
    return it != m_slot_defs.end() ? &it->second : nullptr;
}

std::vector<EquipmentSlotId> EquipmentComponent::all_slots() const {
    std::vector<EquipmentSlotId> result;
    result.reserve(m_slot_defs.size());
    for (const auto& [id, def] : m_slot_defs) {
        result.push_back(id);
    }
    // Sort by UI order
    std::sort(result.begin(), result.end(),
        [this](EquipmentSlotId a, EquipmentSlotId b) {
            const auto* def_a = get_slot_def(a);
            const auto* def_b = get_slot_def(b);
            if (!def_a) return false;
            if (!def_b) return true;
            return def_a->ui_order < def_b->ui_order;
        });
    return result;
}

EquipmentSlotId EquipmentComponent::get_slot_by_type(EquipmentSlotType type) const {
    for (const auto& [id, def] : m_slot_defs) {
        if (def.type == type) {
            return id;
        }
    }
    return EquipmentSlotId{};
}

TransactionResult EquipmentComponent::equip(ItemInstanceId item, EquipmentSlotId slot) {
    if (!can_equip(item, slot)) {
        return TransactionResult::RequirementsNotMet;
    }

    const ItemInstance* inst = get_item_instance(item);
    if (!inst) {
        return TransactionResult::InvalidItem;
    }

    // Get old item for event
    ItemInstanceId old_item;
    std::vector<StatModifier> old_modifiers;
    auto equipped_it = m_equipped.find(slot);
    if (equipped_it != m_equipped.end()) {
        old_item = equipped_it->second.item;
        old_modifiers = equipped_it->second.applied_modifiers;
    }

    // Equip new item
    EquippedItem equipped;
    equipped.slot = slot;
    equipped.item = item;

    auto now = std::chrono::steady_clock::now();
    equipped.equipped_time = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) / 1000.0;

    equipped.applied_modifiers = inst->get_all_stats();
    m_equipped[slot] = equipped;

    // Update set bonuses
    update_set_bonuses();

    // Fire event
    if (m_on_equip) {
        EquipmentChangeEvent event;
        event.entity = m_owner;
        event.slot = slot;
        event.old_item = old_item;
        event.new_item = item;
        event.old_modifiers = old_modifiers;
        event.new_modifiers = equipped.applied_modifiers;
        m_on_equip(event);
    }

    return TransactionResult::Success;
}

TransactionResult EquipmentComponent::equip_auto(ItemInstanceId item, EquipmentSlotId* out_slot) {
    const ItemInstance* inst = get_item_instance(item);
    if (!inst || !inst->def) {
        return TransactionResult::InvalidItem;
    }

    // Find compatible slot
    for (const auto& [id, def] : m_slot_defs) {
        if (def.accepts(*inst->def)) {
            if (can_equip(item, id)) {
                if (out_slot) *out_slot = id;
                return equip(item, id);
            }
        }
    }

    return TransactionResult::ItemNotEquippable;
}

TransactionResult EquipmentComponent::unequip(EquipmentSlotId slot, ItemInstanceId* out_item) {
    auto it = m_equipped.find(slot);
    if (it == m_equipped.end()) {
        return TransactionResult::InvalidSlot;
    }

    EquippedItem old = it->second;
    if (out_item) *out_item = old.item;

    m_equipped.erase(it);

    // Update set bonuses
    update_set_bonuses();

    // Fire event
    if (m_on_unequip) {
        EquipmentChangeEvent event;
        event.entity = m_owner;
        event.slot = slot;
        event.old_item = old.item;
        event.new_item = ItemInstanceId{};
        event.old_modifiers = old.applied_modifiers;
        m_on_unequip(event);
    }

    return TransactionResult::Success;
}

TransactionResult EquipmentComponent::unequip_item(ItemInstanceId item) {
    auto slot = find_item_slot(item);
    if (!slot) {
        return TransactionResult::InvalidItem;
    }
    return unequip(*slot, nullptr);
}

TransactionResult EquipmentComponent::swap_slots(EquipmentSlotId slot_a, EquipmentSlotId slot_b) {
    auto it_a = m_equipped.find(slot_a);
    auto it_b = m_equipped.find(slot_b);

    ItemInstanceId item_a = it_a != m_equipped.end() ? it_a->second.item : ItemInstanceId{};
    ItemInstanceId item_b = it_b != m_equipped.end() ? it_b->second.item : ItemInstanceId{};

    // Check if swap is valid
    if (item_a) {
        const ItemInstance* inst = get_item_instance(item_a);
        const EquipmentSlotDef* def = get_slot_def(slot_b);
        if (!inst || !def || !inst->def || !def->accepts(*inst->def)) {
            return TransactionResult::RequirementsNotMet;
        }
    }
    if (item_b) {
        const ItemInstance* inst = get_item_instance(item_b);
        const EquipmentSlotDef* def = get_slot_def(slot_a);
        if (!inst || !def || !inst->def || !def->accepts(*inst->def)) {
            return TransactionResult::RequirementsNotMet;
        }
    }

    // Perform swap
    if (item_a && item_b) {
        std::swap(m_equipped[slot_a].item, m_equipped[slot_b].item);
    } else if (item_a) {
        m_equipped[slot_b] = m_equipped[slot_a];
        m_equipped[slot_b].slot = slot_b;
        m_equipped.erase(slot_a);
    } else if (item_b) {
        m_equipped[slot_a] = m_equipped[slot_b];
        m_equipped[slot_a].slot = slot_a;
        m_equipped.erase(slot_b);
    }

    update_set_bonuses();
    return TransactionResult::Success;
}

bool EquipmentComponent::can_equip(ItemInstanceId item, EquipmentSlotId slot) const {
    const ItemInstance* inst = get_item_instance(item);
    if (!inst || !inst->def) {
        return false;
    }

    const EquipmentSlotDef* slot_def = get_slot_def(slot);
    if (!slot_def) {
        return false;
    }

    if (!slot_def->accepts(*inst->def)) {
        return false;
    }

    return meets_requirements(*inst);
}

bool EquipmentComponent::meets_requirements(const ItemInstance& item) const {
    if (!item.def) {
        return true;
    }

    for (const auto& req : item.def->requirements) {
        if (m_requirement_checker) {
            if (!m_requirement_checker(req.stat, req.min_value)) {
                return false;
            }
        }
    }

    return true;
}

ItemInstanceId EquipmentComponent::get_equipped(EquipmentSlotId slot) const {
    auto it = m_equipped.find(slot);
    return it != m_equipped.end() ? it->second.item : ItemInstanceId{};
}

std::optional<EquipmentSlotId> EquipmentComponent::find_item_slot(ItemInstanceId item) const {
    for (const auto& [slot, equipped] : m_equipped) {
        if (equipped.item == item) {
            return slot;
        }
    }
    return std::nullopt;
}

bool EquipmentComponent::is_slot_occupied(EquipmentSlotId slot) const {
    return m_equipped.find(slot) != m_equipped.end();
}

std::vector<EquippedItem> EquipmentComponent::all_equipped() const {
    std::vector<EquippedItem> result;
    result.reserve(m_equipped.size());
    for (const auto& [slot, equipped] : m_equipped) {
        result.push_back(equipped);
    }
    return result;
}

std::size_t EquipmentComponent::equipped_count() const {
    return m_equipped.size();
}

std::vector<StatModifier> EquipmentComponent::get_all_modifiers() const {
    std::vector<StatModifier> result;

    // Equipment modifiers
    for (const auto& [slot, equipped] : m_equipped) {
        result.insert(result.end(), equipped.applied_modifiers.begin(), equipped.applied_modifiers.end());
    }

    // Set bonus modifiers
    for (const auto& [set, bonus] : m_active_bonuses) {
        if (bonus) {
            result.insert(result.end(), bonus->stat_bonuses.begin(), bonus->stat_bonuses.end());
        }
    }

    return result;
}

float EquipmentComponent::get_stat_total(StatType stat) const {
    float total = 0;
    for (const auto& mod : get_all_modifiers()) {
        if (mod.stat == stat && mod.type == ModifierType::Flat) {
            total += mod.value;
        }
    }
    return total;
}

std::unordered_map<StatType, float> EquipmentComponent::calculate_stats(
    const std::unordered_map<StatType, float>& base_stats) const {

    std::unordered_map<StatType, float> result = base_stats;
    auto modifiers = get_all_modifiers();

    // Apply flat modifiers first
    for (const auto& mod : modifiers) {
        if (mod.type == ModifierType::Flat) {
            result[mod.stat] += mod.value;
        }
    }

    // Apply percent modifiers
    for (const auto& mod : modifiers) {
        if (mod.type == ModifierType::Percent) {
            auto it = base_stats.find(mod.stat);
            float base = it != base_stats.end() ? it->second : 0;
            result[mod.stat] += base * mod.value;
        }
    }

    // Apply multipliers
    for (const auto& mod : modifiers) {
        if (mod.type == ModifierType::Multiplier) {
            result[mod.stat] *= mod.value;
        }
    }

    return result;
}

std::vector<std::pair<const EquipmentSetDef*, const SetBonus*>> EquipmentComponent::get_active_set_bonuses() const {
    return m_active_bonuses;
}

std::uint32_t EquipmentComponent::get_set_piece_count(const std::string& set_name) const {
    auto it = m_set_counts.find(set_name);
    return it != m_set_counts.end() ? it->second : 0;
}

void EquipmentComponent::set_on_equip(std::function<void(const EquipmentChangeEvent&)> callback) {
    m_on_equip = std::move(callback);
}

void EquipmentComponent::set_on_unequip(std::function<void(const EquipmentChangeEvent&)> callback) {
    m_on_unequip = std::move(callback);
}

std::vector<EquipmentSlotDef> EquipmentComponent::preset_humanoid_slots() {
    std::vector<EquipmentSlotDef> slots;
    std::uint32_t order = 0;

    auto make_slot = [&order](const std::string& name, EquipmentSlotType type,
                             std::vector<EquipmentSlotType> compatible) {
        EquipmentSlotDef def;
        def.name = name;
        def.type = type;
        def.compatible_item_slots = std::move(compatible);
        def.ui_order = order++;
        def.visible = true;
        return def;
    };

    slots.push_back(make_slot("Head", EquipmentSlotType::Head, {EquipmentSlotType::Head}));
    slots.push_back(make_slot("Chest", EquipmentSlotType::Chest, {EquipmentSlotType::Chest}));
    slots.push_back(make_slot("Legs", EquipmentSlotType::Legs, {EquipmentSlotType::Legs}));
    slots.push_back(make_slot("Feet", EquipmentSlotType::Feet, {EquipmentSlotType::Feet}));
    slots.push_back(make_slot("Hands", EquipmentSlotType::Hands, {EquipmentSlotType::Hands}));
    slots.push_back(make_slot("Main Hand", EquipmentSlotType::MainHand,
                             {EquipmentSlotType::MainHand, EquipmentSlotType::TwoHand}));
    slots.push_back(make_slot("Off Hand", EquipmentSlotType::OffHand,
                             {EquipmentSlotType::OffHand}));
    slots.push_back(make_slot("Ring 1", EquipmentSlotType::Ring1, {EquipmentSlotType::Ring1}));
    slots.push_back(make_slot("Ring 2", EquipmentSlotType::Ring2, {EquipmentSlotType::Ring2}));
    slots.push_back(make_slot("Amulet", EquipmentSlotType::Amulet, {EquipmentSlotType::Amulet}));
    slots.push_back(make_slot("Belt", EquipmentSlotType::Belt, {EquipmentSlotType::Belt}));

    return slots;
}

std::vector<EquipmentSlotDef> EquipmentComponent::preset_minimal_slots() {
    std::vector<EquipmentSlotDef> slots;

    EquipmentSlotDef weapon;
    weapon.name = "Weapon";
    weapon.type = EquipmentSlotType::MainHand;
    weapon.compatible_item_slots = {EquipmentSlotType::MainHand, EquipmentSlotType::TwoHand};
    weapon.ui_order = 0;
    slots.push_back(weapon);

    EquipmentSlotDef armor;
    armor.name = "Armor";
    armor.type = EquipmentSlotType::Chest;
    armor.compatible_item_slots = {EquipmentSlotType::Chest};
    armor.ui_order = 1;
    slots.push_back(armor);

    EquipmentSlotDef accessory;
    accessory.name = "Accessory";
    accessory.type = EquipmentSlotType::Trinket1;
    accessory.ui_order = 2;
    slots.push_back(accessory);

    return slots;
}

void EquipmentComponent::update_set_bonuses() {
    if (!m_set_registry) {
        return;
    }

    // Store old bonuses for comparison
    auto old_bonuses = m_active_bonuses;

    // Count equipped pieces per set
    m_set_counts.clear();
    for (const auto& [slot, equipped] : m_equipped) {
        const ItemInstance* inst = get_item_instance(equipped.item);
        if (inst && inst->def && !inst->def->equipment_set.empty()) {
            m_set_counts[inst->def->equipment_set]++;
        }
    }

    // Calculate active bonuses
    m_active_bonuses.clear();
    for (const auto& [set_name, count] : m_set_counts) {
        const EquipmentSetDef* set = m_set_registry->get_set(set_name);
        if (set) {
            const SetBonus* bonus = set->get_active_bonus(count);
            if (bonus) {
                m_active_bonuses.emplace_back(set, bonus);
            }
        }
    }

    // Call activation/deactivation callbacks
    for (const auto& [set, bonus] : old_bonuses) {
        bool still_active = false;
        for (const auto& [new_set, new_bonus] : m_active_bonuses) {
            if (new_set == set && new_bonus == bonus) {
                still_active = true;
                break;
            }
        }
        if (!still_active && bonus && bonus->on_deactivate) {
            bonus->on_deactivate(m_owner);
        }
    }

    for (const auto& [set, bonus] : m_active_bonuses) {
        bool was_active = false;
        for (const auto& [old_set, old_bonus] : old_bonuses) {
            if (old_set == set && old_bonus == bonus) {
                was_active = true;
                break;
            }
        }
        if (!was_active && bonus && bonus->on_activate) {
            bonus->on_activate(m_owner);
        }
    }
}

const ItemInstance* EquipmentComponent::get_item_instance(ItemInstanceId id) const {
    if (!m_item_db || !id) {
        return nullptr;
    }
    auto opt = m_item_db->retrieve(id);
    static thread_local ItemInstance cached;
    if (opt) {
        cached = *opt;
        return &cached;
    }
    return nullptr;
}

// =============================================================================
// LoadoutManager Implementation
// =============================================================================

LoadoutManager::LoadoutManager() = default;

LoadoutManager::LoadoutManager(EquipmentComponent* equipment)
    : m_equipment(equipment) {
}

LoadoutManager::~LoadoutManager() = default;

void LoadoutManager::save_loadout(const std::string& name) {
    if (!m_equipment) return;

    EquipmentLoadout loadout;
    loadout.name = name;

    auto now = std::chrono::steady_clock::now();
    loadout.created_time = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) / 1000.0;

    for (const auto& equipped : m_equipment->all_equipped()) {
        loadout.items[equipped.slot] = equipped.item;
    }

    m_loadouts[name] = std::move(loadout);
}

bool LoadoutManager::apply_loadout(const std::string& name, IContainer* inventory) {
    if (!m_equipment) return false;

    auto it = m_loadouts.find(name);
    if (it == m_loadouts.end()) {
        return false;
    }

    auto& loadout = it->second;

    // Unequip all current items
    for (const auto& slot : m_equipment->all_slots()) {
        ItemInstanceId item;
        m_equipment->unequip(slot, &item);
        if (inventory && item) {
            inventory->add(item, 1, nullptr);
        }
    }

    // Equip loadout items
    for (const auto& [slot, item] : loadout.items) {
        if (inventory) {
            // Try to find item in inventory
            auto inv_slot = inventory->find_item(item);
            if (inv_slot) {
                inventory->remove(*inv_slot, 1);
            }
        }
        m_equipment->equip(item, slot);
    }

    auto now = std::chrono::steady_clock::now();
    loadout.last_used = static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()) / 1000.0;

    return true;
}

bool LoadoutManager::delete_loadout(const std::string& name) {
    return m_loadouts.erase(name) > 0;
}

bool LoadoutManager::rename_loadout(const std::string& old_name, const std::string& new_name) {
    auto it = m_loadouts.find(old_name);
    if (it == m_loadouts.end()) {
        return false;
    }

    auto loadout = std::move(it->second);
    loadout.name = new_name;
    m_loadouts.erase(it);
    m_loadouts[new_name] = std::move(loadout);
    return true;
}

const EquipmentLoadout* LoadoutManager::get_loadout(const std::string& name) const {
    auto it = m_loadouts.find(name);
    return it != m_loadouts.end() ? &it->second : nullptr;
}

std::vector<std::string> LoadoutManager::all_loadouts() const {
    std::vector<std::string> result;
    result.reserve(m_loadouts.size());
    for (const auto& [name, loadout] : m_loadouts) {
        result.push_back(name);
    }
    return result;
}

std::vector<LoadoutManager::SerializedLoadout> LoadoutManager::serialize() const {
    std::vector<SerializedLoadout> result;
    for (const auto& [name, loadout] : m_loadouts) {
        SerializedLoadout s;
        s.name = loadout.name;
        s.created_time = loadout.created_time;
        for (const auto& [slot, item] : loadout.items) {
            s.slot_items.emplace_back(slot.value, item.value);
        }
        result.push_back(std::move(s));
    }
    return result;
}

void LoadoutManager::deserialize(const std::vector<SerializedLoadout>& data) {
    m_loadouts.clear();
    for (const auto& s : data) {
        EquipmentLoadout loadout;
        loadout.name = s.name;
        loadout.created_time = s.created_time;
        for (const auto& [slot_id, item_id] : s.slot_items) {
            loadout.items[EquipmentSlotId{slot_id}] = ItemInstanceId{item_id};
        }
        m_loadouts[loadout.name] = std::move(loadout);
    }
}

// =============================================================================
// CharacterStats Implementation
// =============================================================================

CharacterStats::CharacterStats() = default;
CharacterStats::~CharacterStats() = default;

void CharacterStats::set_base_stat(StatType stat, float value) {
    m_base_stats[stat] = value;
    m_dirty = true;
}

float CharacterStats::get_base_stat(StatType stat) const {
    auto it = m_base_stats.find(stat);
    return it != m_base_stats.end() ? it->second : 0;
}

void CharacterStats::add_modifier(const StatModifier& mod) {
    m_modifiers.push_back(mod);
    m_dirty = true;
}

void CharacterStats::remove_modifiers_by_source(const std::string& source) {
    m_modifiers.erase(
        std::remove_if(m_modifiers.begin(), m_modifiers.end(),
            [&source](const StatModifier& mod) { return mod.source == source; }),
        m_modifiers.end());
    m_dirty = true;
}

void CharacterStats::clear_modifiers() {
    m_modifiers.clear();
    m_dirty = true;
}

float CharacterStats::get_final_stat(StatType stat) const {
    if (m_dirty) {
        const_cast<CharacterStats*>(this)->recalculate();
    }
    auto it = m_final_stats.find(stat);
    return it != m_final_stats.end() ? it->second : 0;
}

void CharacterStats::recalculate() {
    m_final_stats = m_base_stats;

    // Gather all modifiers
    std::vector<StatModifier> all_mods = m_modifiers;
    if (m_equipment) {
        auto equip_mods = m_equipment->get_all_modifiers();
        all_mods.insert(all_mods.end(), equip_mods.begin(), equip_mods.end());
    }

    // Apply flat modifiers
    for (const auto& mod : all_mods) {
        if (mod.type == ModifierType::Flat) {
            m_final_stats[mod.stat] += mod.value;
        }
    }

    // Apply percent modifiers (based on base value)
    for (const auto& mod : all_mods) {
        if (mod.type == ModifierType::Percent) {
            auto it = m_base_stats.find(mod.stat);
            float base = it != m_base_stats.end() ? it->second : 0;
            m_final_stats[mod.stat] += base * mod.value;
        }
    }

    // Apply multipliers
    for (const auto& mod : all_mods) {
        if (mod.type == ModifierType::Multiplier) {
            m_final_stats[mod.stat] *= mod.value;
        }
    }

    m_dirty = false;
}

void CharacterStats::on_equipment_change() {
    m_dirty = true;
}

} // namespace void_inventory
