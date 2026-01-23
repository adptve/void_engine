/// @file crafting.cpp
/// @brief Crafting system implementation for void_inventory module

#include <void_engine/inventory/crafting.hpp>

#include <algorithm>
#include <random>

namespace void_inventory {

// =============================================================================
// RecipeRegistry Implementation
// =============================================================================

RecipeRegistry::RecipeRegistry() = default;
RecipeRegistry::~RecipeRegistry() = default;

RecipeId RecipeRegistry::register_recipe(const Recipe& recipe) {
    RecipeId id{m_next_id++};
    Recipe registered = recipe;
    registered.id = id;

    m_recipes[id] = std::move(registered);

    if (!recipe.name.empty()) {
        m_name_lookup[recipe.name] = id;
    }

    return id;
}

bool RecipeRegistry::unregister_recipe(RecipeId id) {
    auto it = m_recipes.find(id);
    if (it == m_recipes.end()) {
        return false;
    }

    if (!it->second.name.empty()) {
        m_name_lookup.erase(it->second.name);
    }

    m_recipes.erase(it);
    return true;
}

const Recipe* RecipeRegistry::get_recipe(RecipeId id) const {
    auto it = m_recipes.find(id);
    return it != m_recipes.end() ? &it->second : nullptr;
}

RecipeId RecipeRegistry::find_by_name(std::string_view name) const {
    auto it = m_name_lookup.find(std::string(name));
    return it != m_name_lookup.end() ? it->second : RecipeId{};
}

std::vector<RecipeId> RecipeRegistry::find_by_output(ItemDefId output) const {
    std::vector<RecipeId> result;
    for (const auto& [id, recipe] : m_recipes) {
        for (const auto& out : recipe.outputs) {
            if (out.item == output) {
                result.push_back(id);
                break;
            }
        }
    }
    return result;
}

std::vector<RecipeId> RecipeRegistry::find_by_ingredient(ItemDefId ingredient) const {
    std::vector<RecipeId> result;
    for (const auto& [id, recipe] : m_recipes) {
        for (const auto& ing : recipe.ingredients) {
            if (ing.item == ingredient) {
                result.push_back(id);
                break;
            }
        }
    }
    return result;
}

std::vector<RecipeId> RecipeRegistry::find_by_station(StationType station) const {
    std::vector<RecipeId> result;
    for (const auto& [id, recipe] : m_recipes) {
        if (recipe.station_type == station) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<RecipeId> RecipeRegistry::find_by_category(const std::string& category) const {
    std::vector<RecipeId> result;
    for (const auto& [id, recipe] : m_recipes) {
        if (recipe.category == category) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<RecipeId> RecipeRegistry::find_by_tag(std::string_view tag) const {
    std::vector<RecipeId> result;
    for (const auto& [id, recipe] : m_recipes) {
        if (recipe.has_tag(tag)) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<RecipeId> RecipeRegistry::all_recipes() const {
    std::vector<RecipeId> result;
    result.reserve(m_recipes.size());
    for (const auto& [id, recipe] : m_recipes) {
        result.push_back(id);
    }
    return result;
}

std::vector<RecipeId> RecipeRegistry::visible_recipes() const {
    std::vector<RecipeId> result;
    for (const auto& [id, recipe] : m_recipes) {
        if (!recipe.hidden) {
            result.push_back(id);
        }
    }
    return result;
}

void RecipeRegistry::clear() {
    m_recipes.clear();
    m_name_lookup.clear();
}

Recipe RecipeRegistry::preset_iron_sword() {
    Recipe recipe;
    recipe.name = "Iron Sword";
    recipe.description = "Forge an iron sword from iron ingots and wood.";
    recipe.station_type = StationType::Forge;
    recipe.category = "Weapons";
    recipe.tags = {"weapon", "sword", "forge"};
    recipe.difficulty = RecipeDifficulty::Normal;
    recipe.craft_time = 5.0f;

    // Iron ingot x3, wood plank x1
    RecipeIngredient iron;
    iron.quantity = 3;
    recipe.ingredients.push_back(iron);

    RecipeIngredient wood;
    wood.quantity = 1;
    recipe.ingredients.push_back(wood);

    // Output
    RecipeOutput output;
    output.quantity = 1;
    output.base_quality = 1.0f;
    recipe.outputs.push_back(output);

    recipe.experience_granted = 25.0f;
    return recipe;
}

Recipe RecipeRegistry::preset_leather_armor() {
    Recipe recipe;
    recipe.name = "Leather Armor";
    recipe.description = "Craft leather armor from leather pieces.";
    recipe.station_type = StationType::Sewing;
    recipe.category = "Armor";
    recipe.tags = {"armor", "leather", "sewing"};
    recipe.difficulty = RecipeDifficulty::Normal;
    recipe.craft_time = 8.0f;

    RecipeIngredient leather;
    leather.quantity = 5;
    recipe.ingredients.push_back(leather);

    RecipeOutput output;
    output.quantity = 1;
    recipe.outputs.push_back(output);

    recipe.experience_granted = 30.0f;
    return recipe;
}

Recipe RecipeRegistry::preset_health_potion() {
    Recipe recipe;
    recipe.name = "Health Potion";
    recipe.description = "Brew a healing potion from herbs.";
    recipe.station_type = StationType::Alchemy;
    recipe.category = "Potions";
    recipe.tags = {"potion", "healing", "alchemy"};
    recipe.difficulty = RecipeDifficulty::Easy;
    recipe.craft_time = 3.0f;

    RecipeIngredient herb;
    herb.quantity = 2;
    recipe.ingredients.push_back(herb);

    RecipeIngredient water;
    water.quantity = 1;
    recipe.ingredients.push_back(water);

    RecipeOutput output;
    output.quantity = 1;
    recipe.outputs.push_back(output);

    recipe.experience_granted = 10.0f;
    return recipe;
}

Recipe RecipeRegistry::preset_iron_ingot() {
    Recipe recipe;
    recipe.name = "Iron Ingot";
    recipe.description = "Smelt iron ore into an ingot.";
    recipe.station_type = StationType::Forge;
    recipe.category = "Materials";
    recipe.tags = {"material", "metal", "smelting"};
    recipe.difficulty = RecipeDifficulty::Easy;
    recipe.craft_time = 2.0f;

    RecipeIngredient ore;
    ore.quantity = 2;
    recipe.ingredients.push_back(ore);

    RecipeOutput output;
    output.quantity = 1;
    recipe.outputs.push_back(output);

    recipe.experience_granted = 5.0f;
    return recipe;
}

// =============================================================================
// CraftingStation Implementation
// =============================================================================

CraftingStation::CraftingStation() = default;

CraftingStation::CraftingStation(const CraftingStationDef& def)
    : m_def(def) {
}

CraftingStation::~CraftingStation() = default;

bool CraftingStation::start_craft(RecipeId recipe, EntityId crafter) {
    if (!m_recipes) {
        return false;
    }

    if (m_queue.size() >= m_def.max_queue) {
        return false;
    }

    const Recipe* r = m_recipes->get_recipe(recipe);
    if (!r) {
        return false;
    }

    // Check station type
    if (r->station_type != StationType::None && r->station_type != m_def.type) {
        return false;
    }

    // Consume ingredients from input container
    if (!consume_ingredients(*r)) {
        return false;
    }

    CraftingProgress progress;
    progress.recipe = recipe;
    progress.progress = 0;
    progress.total_time = r->craft_time / m_def.speed_multiplier;
    progress.elapsed_time = 0;
    progress.crafter = crafter;
    progress.station = m_def.id;

    m_queue.push(std::move(progress));
    return true;
}

bool CraftingStation::cancel_craft(std::size_t queue_index) {
    if (queue_index >= m_queue.size()) {
        return false;
    }

    // Remove from queue (simple implementation)
    std::queue<CraftingProgress> new_queue;
    std::size_t i = 0;
    while (!m_queue.empty()) {
        if (i != queue_index) {
            new_queue.push(m_queue.front());
        }
        m_queue.pop();
        ++i;
    }
    m_queue = std::move(new_queue);

    return true;
}

const CraftingProgress* CraftingStation::current_progress() const {
    if (m_queue.empty()) {
        return nullptr;
    }
    return &m_queue.front();
}

void CraftingStation::update(float dt) {
    if (m_queue.empty()) {
        return;
    }

    auto& current = m_queue.front();
    if (current.paused) {
        return;
    }

    current.elapsed_time += dt;
    current.progress = current.elapsed_time / current.total_time;

    if (current.elapsed_time >= current.total_time) {
        // Crafting complete
        const Recipe* recipe = m_recipes ? m_recipes->get_recipe(current.recipe) : nullptr;
        if (recipe) {
            CraftingResult result = produce_outputs(*recipe, current.crafter);

            if (m_on_complete) {
                CraftingCompleteEvent event;
                event.crafter = current.crafter;
                event.recipe = current.recipe;
                event.result = result;
                event.quality = 1.0f;  // Would be calculated
                m_on_complete(event);
            }
        }

        m_queue.pop();
    }
}

bool CraftingStation::consume_ingredients(const Recipe& recipe) {
    if (!m_input) {
        return true;  // No input container, assume ingredients provided
    }

    // Check if all ingredients available
    for (const auto& ing : recipe.ingredients) {
        if (m_input->count_item(ing.item) < ing.quantity) {
            return false;
        }
    }

    // Consume ingredients
    for (const auto& ing : recipe.ingredients) {
        if (ing.consumed) {
            auto slots = m_input->find_all(ing.item);
            std::uint32_t remaining = ing.quantity;
            for (auto slot : slots) {
                std::uint32_t in_slot = m_input->get_quantity(slot);
                std::uint32_t to_remove = std::min(remaining, in_slot);
                m_input->remove(slot, to_remove);
                remaining -= to_remove;
                if (remaining == 0) break;
            }
        }
    }

    return true;
}

CraftingResult CraftingStation::produce_outputs(const Recipe& recipe, EntityId /*crafter*/) {
    // Calculate success
    float success_chance = calculate_success_chance(recipe);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0, 1);

    float roll = dist(gen);
    if (roll > success_chance) {
        return CraftingResult::Failure;
    }

    bool critical = roll < success_chance * 0.1f;  // 10% of success is critical

    // Create outputs
    for (const auto& out : recipe.outputs) {
        if (!m_factory || !m_output) {
            continue;
        }

        std::uint32_t quantity = out.quantity;
        if (critical) {
            quantity = static_cast<std::uint32_t>(quantity * 1.5f);
        }

        ItemInstance item = m_factory->create(out.item, quantity);
        item.quality = out.base_quality;

        // Apply variance
        if (out.quality_variance > 0) {
            std::uniform_real_distribution<float> var_dist(-out.quality_variance, out.quality_variance);
            item.quality += var_dist(gen);
            item.quality = std::max(0.1f, std::min(2.0f, item.quality));
        }

        // Store in output container
        m_output->add(item.id, item.quantity, nullptr);
    }

    return critical ? CraftingResult::CriticalSuccess : CraftingResult::Success;
}

float CraftingStation::calculate_success_chance(const Recipe& recipe) const {
    float chance = recipe.success_chance;
    chance += m_def.success_bonus;
    return std::max(0.0f, std::min(1.0f, chance));
}

// =============================================================================
// CraftingComponent Implementation
// =============================================================================

CraftingComponent::CraftingComponent() = default;

CraftingComponent::CraftingComponent(EntityId owner)
    : m_owner(owner) {
}

CraftingComponent::~CraftingComponent() = default;

void CraftingComponent::learn_recipe(RecipeId recipe) {
    if (m_known_recipes.insert(recipe.value).second) {
        if (m_on_learn) {
            m_on_learn(recipe);
        }
    }
}

void CraftingComponent::forget_recipe(RecipeId recipe) {
    m_known_recipes.erase(recipe.value);
}

bool CraftingComponent::knows_recipe(RecipeId recipe) const {
    return m_known_recipes.find(recipe.value) != m_known_recipes.end();
}

std::vector<RecipeId> CraftingComponent::known_recipes() const {
    std::vector<RecipeId> result;
    result.reserve(m_known_recipes.size());
    for (auto id : m_known_recipes) {
        result.push_back(RecipeId{id});
    }
    return result;
}

bool CraftingComponent::discover_recipe(RecipeId recipe) {
    if (!m_recipes) return false;

    const Recipe* r = m_recipes->get_recipe(recipe);
    if (!r || !r->discoverable) {
        return false;
    }

    learn_recipe(recipe);
    return true;
}

void CraftingComponent::set_skill_level(const std::string& skill, std::uint32_t level) {
    m_skill_levels[skill] = level;
}

std::uint32_t CraftingComponent::get_skill_level(const std::string& skill) const {
    auto it = m_skill_levels.find(skill);
    return it != m_skill_levels.end() ? it->second : 0;
}

void CraftingComponent::add_skill_experience(const std::string& skill, float experience) {
    m_skill_experience[skill] += experience;
    // Could implement level-up logic here
}

float CraftingComponent::get_skill_experience(const std::string& skill) const {
    auto it = m_skill_experience.find(skill);
    return it != m_skill_experience.end() ? it->second : 0;
}

bool CraftingComponent::can_craft(RecipeId recipe, IContainer* source) const {
    if (!knows_recipe(recipe)) {
        return false;
    }

    if (!meets_requirements(recipe)) {
        return false;
    }

    if (source && !has_ingredients(recipe, source)) {
        return false;
    }

    return true;
}

bool CraftingComponent::has_ingredients(RecipeId recipe, IContainer* source) const {
    if (!m_recipes || !source) {
        return false;
    }

    const Recipe* r = m_recipes->get_recipe(recipe);
    if (!r) {
        return false;
    }

    for (const auto& ing : r->ingredients) {
        if (source->count_item(ing.item) < ing.quantity) {
            return false;
        }
    }

    return true;
}

bool CraftingComponent::meets_requirements(RecipeId recipe) const {
    if (!m_recipes) {
        return true;
    }

    const Recipe* r = m_recipes->get_recipe(recipe);
    if (!r) {
        return false;
    }

    // Check skill level
    if (!r->required_skill.empty()) {
        if (get_skill_level(r->required_skill) < r->required_skill_level) {
            return false;
        }
    }

    // Check max crafts
    if (r->max_crafts > 0) {
        if (get_craft_count(recipe) >= r->max_crafts) {
            return false;
        }
    }

    return true;
}

std::vector<std::pair<ItemDefId, std::uint32_t>> CraftingComponent::get_missing_ingredients(
    RecipeId recipe, IContainer* source) const {

    std::vector<std::pair<ItemDefId, std::uint32_t>> missing;

    if (!m_recipes || !source) {
        return missing;
    }

    const Recipe* r = m_recipes->get_recipe(recipe);
    if (!r) {
        return missing;
    }

    for (const auto& ing : r->ingredients) {
        std::uint32_t have = source->count_item(ing.item);
        if (have < ing.quantity) {
            missing.emplace_back(ing.item, ing.quantity - have);
        }
    }

    return missing;
}

CraftingResult CraftingComponent::craft_instant(RecipeId recipe, IContainer* source, IContainer* dest) {
    if (!can_craft(recipe, source)) {
        return CraftingResult::InsufficientMaterials;
    }

    const Recipe* r = m_recipes->get_recipe(recipe);
    if (!r) {
        return CraftingResult::InvalidRecipe;
    }

    // Consume ingredients
    for (const auto& ing : r->ingredients) {
        if (ing.consumed) {
            auto slots = source->find_all(ing.item);
            std::uint32_t remaining = ing.quantity;
            for (auto slot : slots) {
                std::uint32_t in_slot = source->get_quantity(slot);
                std::uint32_t to_remove = std::min(remaining, in_slot);
                source->remove(slot, to_remove);
                remaining -= to_remove;
                if (remaining == 0) break;
            }
        }
    }

    // Calculate success
    float success_chance = r->success_chance + m_success_bonus;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0, 1);

    float roll = dist(gen);
    if (roll > success_chance) {
        return CraftingResult::Failure;
    }

    bool critical = roll < success_chance * 0.1f;

    // Produce outputs
    std::vector<ItemInstanceId> output_items;
    for (const auto& out : r->outputs) {
        if (!m_factory) continue;

        std::uint32_t quantity = out.quantity;
        if (critical) {
            quantity = static_cast<std::uint32_t>(quantity * 1.5f);
        }

        ItemInstance item = m_factory->create(out.item, quantity);
        item.quality = out.base_quality;

        if (dest) {
            dest->add(item.id, item.quantity, nullptr);
        }
        output_items.push_back(item.id);
    }

    // Update stats
    m_craft_counts[recipe.value]++;
    m_total_crafts++;

    // Grant experience
    if (!r->required_skill.empty()) {
        add_skill_experience(r->required_skill, r->experience_granted);
    }

    // Fire callback
    if (m_on_craft) {
        CraftingCompleteEvent event;
        event.crafter = m_owner;
        event.recipe = recipe;
        event.result = critical ? CraftingResult::CriticalSuccess : CraftingResult::Success;
        event.outputs = output_items;
        event.quality = 1.0f;
        m_on_craft(event);
    }

    return critical ? CraftingResult::CriticalSuccess : CraftingResult::Success;
}

bool CraftingComponent::start_craft(RecipeId recipe, CraftingStation* station) {
    if (!station || !can_craft(recipe, station->input_container())) {
        return false;
    }

    return station->start_craft(recipe, m_owner);
}

std::vector<CraftingProgress> CraftingComponent::active_crafts() const {
    // Would track active crafts at stations
    return {};
}

std::uint32_t CraftingComponent::get_craft_count(RecipeId recipe) const {
    auto it = m_craft_counts.find(recipe.value);
    return it != m_craft_counts.end() ? it->second : 0;
}

// =============================================================================
// CraftingQueue Implementation
// =============================================================================

CraftingQueue::CraftingQueue() = default;
CraftingQueue::~CraftingQueue() = default;

bool CraftingQueue::queue(RecipeId recipe, std::uint32_t count, EntityId crafter, CraftingStationId station) {
    if (!m_recipes) {
        return false;
    }

    const Recipe* r = m_recipes->get_recipe(recipe);
    if (!r) {
        return false;
    }

    QueuedCraft craft;
    craft.recipe = recipe;
    craft.count = count;
    craft.progress = 0;
    craft.total_time = r->craft_time * count;
    craft.crafter = crafter;
    craft.station = station;
    craft.paused = false;

    m_queue.push_back(std::move(craft));
    return true;
}

bool CraftingQueue::cancel(std::size_t index) {
    if (index >= m_queue.size()) {
        return false;
    }
    m_queue.erase(m_queue.begin() + index);
    return true;
}

void CraftingQueue::cancel_all() {
    m_queue.clear();
}

void CraftingQueue::pause(std::size_t index) {
    if (index < m_queue.size()) {
        m_queue[index].paused = true;
    }
}

void CraftingQueue::resume(std::size_t index) {
    if (index < m_queue.size()) {
        m_queue[index].paused = false;
    }
}

void CraftingQueue::pause_all() {
    for (auto& craft : m_queue) {
        craft.paused = true;
    }
}

void CraftingQueue::resume_all() {
    for (auto& craft : m_queue) {
        craft.paused = false;
    }
}

bool CraftingQueue::move_up(std::size_t index) {
    if (index == 0 || index >= m_queue.size()) {
        return false;
    }
    std::swap(m_queue[index], m_queue[index - 1]);
    return true;
}

bool CraftingQueue::move_down(std::size_t index) {
    if (index + 1 >= m_queue.size()) {
        return false;
    }
    std::swap(m_queue[index], m_queue[index + 1]);
    return true;
}

void CraftingQueue::update(float dt) {
    if (m_queue.empty()) {
        return;
    }

    auto& current = m_queue.front();
    if (current.paused) {
        return;
    }

    float time_per_item = current.total_time / current.count;
    float old_progress = current.progress;
    current.progress += dt / current.total_time;

    // Check for completions
    std::uint32_t old_completed = static_cast<std::uint32_t>(old_progress * current.count);
    std::uint32_t new_completed = static_cast<std::uint32_t>(current.progress * current.count);

    for (std::uint32_t i = old_completed; i < new_completed && i < current.count; ++i) {
        if (m_on_complete) {
            CraftingCompleteEvent event;
            event.crafter = current.crafter;
            event.recipe = current.recipe;
            event.result = CraftingResult::Success;
            m_on_complete(event);
        }
    }

    if (current.progress >= 1.0f) {
        m_queue.erase(m_queue.begin());
    }
}

// =============================================================================
// CraftingPreviewer Implementation
// =============================================================================

CraftingPreviewer::CraftingPreviewer() = default;
CraftingPreviewer::~CraftingPreviewer() = default;

CraftingPreview CraftingPreviewer::preview(RecipeId recipe, const CraftingComponent* crafter,
                                           IContainer* source, CraftingStation* station) const {
    CraftingPreview preview;
    preview.recipe = recipe;

    if (!m_recipes) {
        return preview;
    }

    const Recipe* r = m_recipes->get_recipe(recipe);
    if (!r) {
        return preview;
    }

    // Check ingredients
    if (source) {
        preview.has_ingredients = true;
        for (const auto& ing : r->ingredients) {
            std::uint32_t have = source->count_item(ing.item);
            if (have < ing.quantity) {
                preview.has_ingredients = false;
                preview.missing_ingredients.emplace_back(ing.item, ing.quantity - have);
            }
        }
    }

    // Check station
    if (r->station_type == StationType::None) {
        preview.has_station = true;
    } else if (station && station->type() == r->station_type) {
        preview.has_station = true;
    }

    // Check skill
    preview.required_skill_level = r->required_skill_level;
    if (crafter && !r->required_skill.empty()) {
        preview.current_skill_level = crafter->get_skill_level(r->required_skill);
        preview.has_skill = preview.current_skill_level >= r->required_skill_level;
    } else {
        preview.has_skill = true;
    }

    // Check unlocks
    preview.has_unlocks = true;
    for (const auto& unlock : r->required_unlocks) {
        // Would check against player progress
        preview.missing_unlocks.push_back(unlock);
        preview.has_unlocks = false;
    }

    // Calculate success and time
    preview.success_chance = r->success_chance;
    if (crafter) {
        preview.success_chance += crafter->success_bonus();
    }
    if (station) {
        preview.success_chance += station->success_bonus();
    }
    preview.success_chance = std::max(0.0f, std::min(1.0f, preview.success_chance));

    preview.craft_time = r->craft_time;
    if (crafter) {
        preview.craft_time /= (1.0f + crafter->speed_bonus());
    }
    if (station) {
        preview.craft_time /= station->speed_multiplier();
    }

    preview.expected_outputs = r->outputs;
    preview.experience_gain = r->experience_granted;

    preview.can_craft = preview.has_ingredients && preview.has_station &&
                        preview.has_skill && preview.has_unlocks;

    return preview;
}

std::vector<CraftingPreview> CraftingPreviewer::preview_all(const CraftingComponent* crafter,
                                                            IContainer* source,
                                                            CraftingStation* station) const {
    std::vector<CraftingPreview> result;

    if (!m_recipes || !crafter) {
        return result;
    }

    for (const auto& recipe : crafter->known_recipes()) {
        result.push_back(preview(recipe, crafter, source, station));
    }

    return result;
}

} // namespace void_inventory
