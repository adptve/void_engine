/// @file crafting.hpp
/// @brief Crafting system for void_inventory module

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "items.hpp"
#include "containers.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace void_inventory {

// =============================================================================
// Recipe
// =============================================================================

/// @brief Recipe definition
struct Recipe {
    RecipeId id;
    std::string name;
    std::string description;
    std::string icon_path;

    // Requirements
    std::vector<RecipeIngredient> ingredients;
    StationType station_type{StationType::None};    ///< None = no station required
    std::uint32_t required_skill_level{0};
    std::string required_skill;
    std::vector<std::string> required_unlocks;      ///< Prerequisites

    // Output
    std::vector<RecipeOutput> outputs;

    // Timing
    float craft_time{1.0f};                         ///< Base craft time in seconds
    bool instant{false};                            ///< Skip time requirement

    // Difficulty
    RecipeDifficulty difficulty{RecipeDifficulty::Normal};
    float success_chance{1.0f};                     ///< Base success chance

    // Categorization
    std::string category;
    std::vector<std::string> tags;

    // Flags
    bool hidden{false};                             ///< Hidden until discovered
    bool discoverable{true};                        ///< Can be discovered
    bool repeatable{true};                          ///< Can craft multiple times
    std::uint32_t max_crafts{0};                    ///< 0 = unlimited

    // Experience
    float experience_granted{0};

    bool has_tag(std::string_view tag) const {
        for (const auto& t : tags) {
            if (t == tag) return true;
        }
        return false;
    }

    /// @brief Calculate total ingredient count
    std::uint32_t total_ingredient_count() const {
        std::uint32_t total = 0;
        for (const auto& ing : ingredients) {
            total += ing.quantity;
        }
        return total;
    }
};

// =============================================================================
// RecipeRegistry
// =============================================================================

/// @brief Registry for crafting recipes
class RecipeRegistry {
public:
    RecipeRegistry();
    ~RecipeRegistry();

    /// @brief Register a new recipe
    RecipeId register_recipe(const Recipe& recipe);

    /// @brief Unregister a recipe
    bool unregister_recipe(RecipeId id);

    /// @brief Get recipe by ID
    const Recipe* get_recipe(RecipeId id) const;

    /// @brief Find recipe by name
    RecipeId find_by_name(std::string_view name) const;

    /// @brief Find recipes that produce an item
    std::vector<RecipeId> find_by_output(ItemDefId output) const;

    /// @brief Find recipes that use an item as ingredient
    std::vector<RecipeId> find_by_ingredient(ItemDefId ingredient) const;

    /// @brief Find recipes for a station type
    std::vector<RecipeId> find_by_station(StationType station) const;

    /// @brief Find recipes by category
    std::vector<RecipeId> find_by_category(const std::string& category) const;

    /// @brief Find recipes by tag
    std::vector<RecipeId> find_by_tag(std::string_view tag) const;

    /// @brief Get all recipe IDs
    std::vector<RecipeId> all_recipes() const;

    /// @brief Get visible recipes
    std::vector<RecipeId> visible_recipes() const;

    /// @brief Get recipe count
    std::size_t count() const { return m_recipes.size(); }

    /// @brief Clear all recipes
    void clear();

    // Preset recipes
    static Recipe preset_iron_sword();
    static Recipe preset_leather_armor();
    static Recipe preset_health_potion();
    static Recipe preset_iron_ingot();

private:
    std::unordered_map<RecipeId, Recipe> m_recipes;
    std::unordered_map<std::string, RecipeId> m_name_lookup;
    std::uint64_t m_next_id{1};
};

// =============================================================================
// CraftingStation
// =============================================================================

/// @brief Crafting station definition
struct CraftingStationDef {
    CraftingStationId id;
    std::string name;
    StationType type{StationType::Basic};
    std::uint32_t tier{1};                          ///< Station quality tier
    float speed_multiplier{1.0f};
    float success_bonus{0};
    std::uint32_t max_queue{1};                     ///< Max queued crafts
    std::vector<std::string> unlocked_recipes;      ///< Specific recipes enabled
};

/// @brief Active crafting station instance
class CraftingStation {
public:
    CraftingStation();
    explicit CraftingStation(const CraftingStationDef& def);
    ~CraftingStation();

    // Identity
    CraftingStationId id() const { return m_def.id; }
    const std::string& name() const { return m_def.name; }
    StationType type() const { return m_def.type; }
    std::uint32_t tier() const { return m_def.tier; }

    // Properties
    float speed_multiplier() const { return m_def.speed_multiplier; }
    float success_bonus() const { return m_def.success_bonus; }
    std::uint32_t max_queue() const { return m_def.max_queue; }

    // Queue management
    /// @brief Start crafting a recipe
    bool start_craft(RecipeId recipe, EntityId crafter);

    /// @brief Cancel current or queued craft
    bool cancel_craft(std::size_t queue_index = 0);

    /// @brief Get current crafting progress
    const CraftingProgress* current_progress() const;

    /// @brief Get queue size
    std::size_t queue_size() const { return m_queue.size(); }

    /// @brief Check if station is busy
    bool is_busy() const { return !m_queue.empty(); }

    /// @brief Update crafting progress
    void update(float dt);

    // Dependencies
    void set_recipe_registry(RecipeRegistry* registry) { m_recipes = registry; }
    void set_item_factory(ItemFactory* factory) { m_factory = factory; }

    // Callbacks
    using CraftCompleteCallback = std::function<void(const CraftingCompleteEvent&)>;
    void set_on_complete(CraftCompleteCallback callback) { m_on_complete = std::move(callback); }

    // Input/output containers
    void set_input_container(IContainer* container) { m_input = container; }
    void set_output_container(IContainer* container) { m_output = container; }
    IContainer* input_container() const { return m_input; }
    IContainer* output_container() const { return m_output; }

    // Position (for world placement)
    void set_position(float x, float y, float z) { m_x = x; m_y = y; m_z = z; }
    void get_position(float& x, float& y, float& z) const { x = m_x; y = m_y; z = m_z; }

private:
    bool consume_ingredients(const Recipe& recipe);
    CraftingResult produce_outputs(const Recipe& recipe, EntityId crafter);
    float calculate_success_chance(const Recipe& recipe) const;

    CraftingStationDef m_def;
    std::queue<CraftingProgress> m_queue;

    RecipeRegistry* m_recipes{nullptr};
    ItemFactory* m_factory{nullptr};
    IContainer* m_input{nullptr};
    IContainer* m_output{nullptr};

    CraftCompleteCallback m_on_complete;

    float m_x{0}, m_y{0}, m_z{0};
};

// =============================================================================
// CraftingComponent
// =============================================================================

/// @brief Component for entity crafting capabilities
class CraftingComponent {
public:
    CraftingComponent();
    explicit CraftingComponent(EntityId owner);
    ~CraftingComponent();

    // Recipe knowledge
    /// @brief Learn a recipe
    void learn_recipe(RecipeId recipe);

    /// @brief Forget a recipe
    void forget_recipe(RecipeId recipe);

    /// @brief Check if recipe is known
    bool knows_recipe(RecipeId recipe) const;

    /// @brief Get all known recipes
    std::vector<RecipeId> known_recipes() const;

    /// @brief Discover a recipe (auto-learn if discoverable)
    bool discover_recipe(RecipeId recipe);

    // Skills
    /// @brief Set skill level
    void set_skill_level(const std::string& skill, std::uint32_t level);

    /// @brief Get skill level
    std::uint32_t get_skill_level(const std::string& skill) const;

    /// @brief Add experience to skill
    void add_skill_experience(const std::string& skill, float experience);

    /// @brief Get skill experience
    float get_skill_experience(const std::string& skill) const;

    // Crafting
    /// @brief Check if can craft recipe
    bool can_craft(RecipeId recipe, IContainer* source = nullptr) const;

    /// @brief Check if has ingredients
    bool has_ingredients(RecipeId recipe, IContainer* source) const;

    /// @brief Check if meets requirements
    bool meets_requirements(RecipeId recipe) const;

    /// @brief Get missing ingredients
    std::vector<std::pair<ItemDefId, std::uint32_t>> get_missing_ingredients(RecipeId recipe, IContainer* source) const;

    /// @brief Craft instantly (no station)
    CraftingResult craft_instant(RecipeId recipe, IContainer* source, IContainer* dest);

    /// @brief Start crafting at station
    bool start_craft(RecipeId recipe, CraftingStation* station);

    /// @brief Get active crafts
    std::vector<CraftingProgress> active_crafts() const;

    // Stats
    float success_bonus() const { return m_success_bonus; }
    void set_success_bonus(float bonus) { m_success_bonus = bonus; }

    float speed_bonus() const { return m_speed_bonus; }
    void set_speed_bonus(float bonus) { m_speed_bonus = bonus; }

    // Dependencies
    void set_recipe_registry(RecipeRegistry* registry) { m_recipes = registry; }
    void set_item_factory(ItemFactory* factory) { m_factory = factory; }
    void set_item_database(ItemDatabase* db) { m_item_db = db; }

    // Callbacks
    void set_on_learn(std::function<void(RecipeId)> callback) { m_on_learn = std::move(callback); }
    void set_on_craft(CraftingCompleteCallback callback) { m_on_craft = std::move(callback); }

    // Owner
    EntityId owner() const { return m_owner; }

    // Craft counts
    std::uint32_t get_craft_count(RecipeId recipe) const;
    std::uint32_t total_crafts() const { return m_total_crafts; }

private:
    EntityId m_owner;

    std::unordered_set<std::uint64_t> m_known_recipes;
    std::unordered_map<std::string, std::uint32_t> m_skill_levels;
    std::unordered_map<std::string, float> m_skill_experience;
    std::unordered_map<std::uint64_t, std::uint32_t> m_craft_counts;

    float m_success_bonus{0};
    float m_speed_bonus{0};
    std::uint32_t m_total_crafts{0};

    RecipeRegistry* m_recipes{nullptr};
    ItemFactory* m_factory{nullptr};
    ItemDatabase* m_item_db{nullptr};

    std::function<void(RecipeId)> m_on_learn;
    CraftingCompleteCallback m_on_craft;
};

// =============================================================================
// CraftingQueue - Multi-craft queue
// =============================================================================

/// @brief Manages multiple craft operations
class CraftingQueue {
public:
    struct QueuedCraft {
        RecipeId recipe;
        std::uint32_t count{1};
        float progress{0};
        float total_time{0};
        EntityId crafter;
        CraftingStationId station;
        bool paused{false};
    };

    CraftingQueue();
    ~CraftingQueue();

    /// @brief Queue a craft
    bool queue(RecipeId recipe, std::uint32_t count, EntityId crafter, CraftingStationId station);

    /// @brief Cancel queued craft
    bool cancel(std::size_t index);

    /// @brief Cancel all
    void cancel_all();

    /// @brief Pause/resume
    void pause(std::size_t index);
    void resume(std::size_t index);
    void pause_all();
    void resume_all();

    /// @brief Move in queue
    bool move_up(std::size_t index);
    bool move_down(std::size_t index);

    /// @brief Get queue
    const std::vector<QueuedCraft>& queue() const { return m_queue; }

    /// @brief Get queue size
    std::size_t size() const { return m_queue.size(); }

    /// @brief Check if empty
    bool empty() const { return m_queue.empty(); }

    /// @brief Update queue
    void update(float dt);

    // Dependencies
    void set_recipe_registry(RecipeRegistry* registry) { m_recipes = registry; }

    // Callbacks
    void set_on_complete(CraftingCompleteCallback callback) { m_on_complete = std::move(callback); }

private:
    std::vector<QueuedCraft> m_queue;
    RecipeRegistry* m_recipes{nullptr};
    CraftingCompleteCallback m_on_complete;
};

// =============================================================================
// CraftingPreview - Preview craft results
// =============================================================================

/// @brief Preview of craft results
struct CraftingPreview {
    RecipeId recipe;
    bool can_craft{false};

    // Requirements
    bool has_ingredients{false};
    bool has_station{false};
    bool has_skill{false};
    bool has_unlocks{false};

    // Missing
    std::vector<std::pair<ItemDefId, std::uint32_t>> missing_ingredients;
    std::uint32_t required_skill_level{0};
    std::uint32_t current_skill_level{0};
    std::vector<std::string> missing_unlocks;

    // Results
    float success_chance{0};
    float craft_time{0};
    std::vector<RecipeOutput> expected_outputs;

    // Experience
    float experience_gain{0};
};

/// @brief Generates crafting previews
class CraftingPreviewer {
public:
    CraftingPreviewer();
    ~CraftingPreviewer();

    /// @brief Generate preview for a recipe
    CraftingPreview preview(RecipeId recipe, const CraftingComponent* crafter,
                           IContainer* source, CraftingStation* station = nullptr) const;

    /// @brief Preview all available recipes
    std::vector<CraftingPreview> preview_all(const CraftingComponent* crafter,
                                             IContainer* source,
                                             CraftingStation* station = nullptr) const;

    void set_recipe_registry(RecipeRegistry* registry) { m_recipes = registry; }
    void set_item_database(ItemDatabase* db) { m_item_db = db; }

private:
    RecipeRegistry* m_recipes{nullptr};
    ItemDatabase* m_item_db{nullptr};
};

} // namespace void_inventory
