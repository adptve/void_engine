/// @file containers.hpp
/// @brief Container systems for void_inventory module

#pragma once

#include "fwd.hpp"
#include "types.hpp"
#include "items.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace void_inventory {

// =============================================================================
// IContainer Interface
// =============================================================================

/// @brief Interface for all container types
class IContainer {
public:
    virtual ~IContainer() = default;

    // Identity
    virtual ContainerId id() const = 0;
    virtual ContainerType type() const = 0;
    virtual std::string_view name() const = 0;

    // Capacity
    virtual std::size_t capacity() const = 0;
    virtual std::size_t size() const = 0;
    virtual std::size_t free_slots() const = 0;
    virtual bool empty() const = 0;
    virtual bool full() const = 0;

    // Access
    virtual const SlotState* get_slot(std::uint32_t index) const = 0;
    virtual ItemInstanceId get_item(std::uint32_t slot) const = 0;
    virtual std::uint32_t get_quantity(std::uint32_t slot) const = 0;

    // Operations
    virtual TransactionResult add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot = nullptr) = 0;
    virtual TransactionResult add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) = 0;
    virtual TransactionResult remove(std::uint32_t slot, std::uint32_t quantity) = 0;
    virtual TransactionResult remove_item(ItemInstanceId item, std::uint32_t quantity) = 0;
    virtual TransactionResult move(std::uint32_t from_slot, std::uint32_t to_slot) = 0;
    virtual TransactionResult swap(std::uint32_t slot_a, std::uint32_t slot_b) = 0;

    // Queries
    virtual std::optional<std::uint32_t> find_item(ItemInstanceId item) const = 0;
    virtual std::optional<std::uint32_t> find_item_def(ItemDefId def) const = 0;
    virtual std::optional<std::uint32_t> find_empty_slot() const = 0;
    virtual std::vector<std::uint32_t> find_all(ItemDefId def) const = 0;
    virtual std::uint32_t count_item(ItemDefId def) const = 0;
    virtual bool contains(ItemInstanceId item) const = 0;
    virtual bool contains_def(ItemDefId def) const = 0;

    // Bulk operations
    virtual void clear() = 0;
    virtual void sort() = 0;
    virtual void compact() = 0;  ///< Move items to fill gaps

    // Events
    virtual void set_on_change(ContainerChangeCallback callback) = 0;
    virtual void set_filter(ItemFilterCallback filter) = 0;
    virtual void set_slot_filter(SlotFilterCallback filter) = 0;
};

// =============================================================================
// Container - Basic Slot-Based Container
// =============================================================================

/// @brief Basic slot-based inventory container
class Container : public IContainer {
public:
    Container();
    Container(ContainerId id, std::size_t capacity);
    Container(ContainerId id, const std::string& name, std::size_t capacity);
    ~Container() override;

    // Move/copy
    Container(Container&&) noexcept;
    Container& operator=(Container&&) noexcept;
    Container(const Container&) = delete;
    Container& operator=(const Container&) = delete;

    // Identity
    ContainerId id() const override { return m_id; }
    ContainerType type() const override { return ContainerType::Basic; }
    std::string_view name() const override { return m_name; }

    // Capacity
    std::size_t capacity() const override { return m_slots.size(); }
    std::size_t size() const override;
    std::size_t free_slots() const override;
    bool empty() const override;
    bool full() const override;

    // Access
    const SlotState* get_slot(std::uint32_t index) const override;
    ItemInstanceId get_item(std::uint32_t slot) const override;
    std::uint32_t get_quantity(std::uint32_t slot) const override;

    // Operations
    TransactionResult add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot = nullptr) override;
    TransactionResult add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) override;
    TransactionResult remove(std::uint32_t slot, std::uint32_t quantity) override;
    TransactionResult remove_item(ItemInstanceId item, std::uint32_t quantity) override;
    TransactionResult move(std::uint32_t from_slot, std::uint32_t to_slot) override;
    TransactionResult swap(std::uint32_t slot_a, std::uint32_t slot_b) override;

    // Queries
    std::optional<std::uint32_t> find_item(ItemInstanceId item) const override;
    std::optional<std::uint32_t> find_item_def(ItemDefId def) const override;
    std::optional<std::uint32_t> find_empty_slot() const override;
    std::vector<std::uint32_t> find_all(ItemDefId def) const override;
    std::uint32_t count_item(ItemDefId def) const override;
    bool contains(ItemInstanceId item) const override;
    bool contains_def(ItemDefId def) const override;

    // Bulk operations
    void clear() override;
    void sort() override;
    void compact() override;

    // Events
    void set_on_change(ContainerChangeCallback callback) override { m_on_change = std::move(callback); }
    void set_filter(ItemFilterCallback filter) override { m_filter = std::move(filter); }
    void set_slot_filter(SlotFilterCallback filter) override { m_slot_filter = std::move(filter); }

    // Additional
    void resize(std::size_t new_capacity);
    void configure_slot(std::uint32_t slot, const SlotConfig& config);
    const SlotConfig* get_slot_config(std::uint32_t slot) const;

    // Database integration
    void set_item_database(ItemDatabase* db) { m_item_db = db; }

protected:
    void notify_change(std::uint32_t slot);
    bool passes_filter(const ItemInstance& item) const;
    bool passes_slot_filter(const ItemInstance& item, std::uint32_t slot) const;
    const ItemInstance* get_item_instance(ItemInstanceId id) const;

    ContainerId m_id;
    std::string m_name;
    std::vector<SlotState> m_slots;
    std::unordered_map<std::uint32_t, SlotConfig> m_slot_configs;

    ItemDatabase* m_item_db{nullptr};
    ContainerChangeCallback m_on_change;
    ItemFilterCallback m_filter;
    SlotFilterCallback m_slot_filter;
};

// =============================================================================
// GridContainer - 2D Grid-Based Container (Diablo-style)
// =============================================================================

/// @brief 2D grid-based inventory (items occupy multiple cells)
class GridContainer : public IContainer {
public:
    GridContainer();
    GridContainer(ContainerId id, std::uint32_t width, std::uint32_t height);
    ~GridContainer() override;

    // IContainer implementation
    ContainerId id() const override { return m_id; }
    ContainerType type() const override { return ContainerType::Grid; }
    std::string_view name() const override { return m_name; }

    std::size_t capacity() const override { return m_width * m_height; }
    std::size_t size() const override;
    std::size_t free_slots() const override;
    bool empty() const override;
    bool full() const override;

    const SlotState* get_slot(std::uint32_t index) const override;
    ItemInstanceId get_item(std::uint32_t slot) const override;
    std::uint32_t get_quantity(std::uint32_t slot) const override;

    TransactionResult add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot = nullptr) override;
    TransactionResult add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) override;
    TransactionResult remove(std::uint32_t slot, std::uint32_t quantity) override;
    TransactionResult remove_item(ItemInstanceId item, std::uint32_t quantity) override;
    TransactionResult move(std::uint32_t from_slot, std::uint32_t to_slot) override;
    TransactionResult swap(std::uint32_t slot_a, std::uint32_t slot_b) override;

    std::optional<std::uint32_t> find_item(ItemInstanceId item) const override;
    std::optional<std::uint32_t> find_item_def(ItemDefId def) const override;
    std::optional<std::uint32_t> find_empty_slot() const override;
    std::vector<std::uint32_t> find_all(ItemDefId def) const override;
    std::uint32_t count_item(ItemDefId def) const override;
    bool contains(ItemInstanceId item) const override;
    bool contains_def(ItemDefId def) const override;

    void clear() override;
    void sort() override;
    void compact() override;

    void set_on_change(ContainerChangeCallback callback) override { m_on_change = std::move(callback); }
    void set_filter(ItemFilterCallback filter) override { m_filter = std::move(filter); }
    void set_slot_filter(SlotFilterCallback filter) override { m_slot_filter = std::move(filter); }

    // Grid-specific
    std::uint32_t width() const { return m_width; }
    std::uint32_t height() const { return m_height; }

    /// @brief Add item at specific grid position
    TransactionResult add_at(ItemInstanceId item, std::uint32_t x, std::uint32_t y);

    /// @brief Check if area is free
    bool is_area_free(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) const;

    /// @brief Find first free position for item size
    std::optional<GridPosition> find_free_position(std::uint32_t w, std::uint32_t h) const;

    /// @brief Get item at grid position
    ItemInstanceId get_at(std::uint32_t x, std::uint32_t y) const;

    /// @brief Convert grid position to slot index
    std::uint32_t grid_to_slot(std::uint32_t x, std::uint32_t y) const { return y * m_width + x; }

    /// @brief Convert slot index to grid position
    GridPosition slot_to_grid(std::uint32_t slot) const {
        return {slot % m_width, slot / m_width};
    }

    void set_item_database(ItemDatabase* db) { m_item_db = db; }

private:
    struct GridItem {
        ItemInstanceId item;
        std::uint32_t quantity{0};
        GridPosition position;
        GridSize size;
    };

    void mark_cells(const GridPosition& pos, const GridSize& size, ItemInstanceId item);
    void clear_cells(const GridPosition& pos, const GridSize& size);
    const ItemInstance* get_item_instance(ItemInstanceId id) const;

    ContainerId m_id;
    std::string m_name;
    std::uint32_t m_width{0};
    std::uint32_t m_height{0};

    std::vector<ItemInstanceId> m_grid;  // Cell occupancy
    std::vector<GridItem> m_items;

    ItemDatabase* m_item_db{nullptr};
    ContainerChangeCallback m_on_change;
    ItemFilterCallback m_filter;
    SlotFilterCallback m_slot_filter;
};

// =============================================================================
// WeightedContainer - Weight-Limited Container
// =============================================================================

/// @brief Container with weight limit
class WeightedContainer : public Container {
public:
    WeightedContainer();
    WeightedContainer(ContainerId id, std::size_t capacity, float weight_limit);
    ~WeightedContainer() override;

    ContainerType type() const override { return ContainerType::Weighted; }

    TransactionResult add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot = nullptr) override;
    TransactionResult add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) override;

    // Weight management
    float weight_limit() const { return m_weight_limit; }
    void set_weight_limit(float limit) { m_weight_limit = limit; }
    float current_weight() const { return m_current_weight; }
    float available_weight() const { return m_weight_limit - m_current_weight; }
    float weight_percent() const { return m_weight_limit > 0 ? m_current_weight / m_weight_limit : 0; }
    bool is_overweight() const { return m_current_weight > m_weight_limit; }

    /// @brief Check if item can fit by weight
    bool can_fit_weight(float item_weight, std::uint32_t quantity) const;

    /// @brief Recalculate total weight
    void recalculate_weight();

private:
    float m_weight_limit{100.0f};
    float m_current_weight{0};
};

// =============================================================================
// FilteredContainer - Auto-Filtering Container
// =============================================================================

/// @brief Container that only accepts certain items
class FilteredContainer : public Container {
public:
    FilteredContainer();
    FilteredContainer(ContainerId id, std::size_t capacity);
    ~FilteredContainer() override;

    /// @brief Set allowed categories
    void set_allowed_categories(const std::vector<ItemCategory>& categories);

    /// @brief Set allowed item definitions
    void set_allowed_items(const std::vector<ItemDefId>& items);

    /// @brief Set required tags
    void set_required_tags(const std::vector<std::string>& tags);

    /// @brief Check if item is allowed
    bool is_allowed(const ItemInstance& item) const;

    TransactionResult add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot = nullptr) override;
    TransactionResult add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) override;

private:
    std::vector<ItemCategory> m_allowed_categories;
    std::vector<ItemDefId> m_allowed_items;
    std::vector<std::string> m_required_tags;
};

// =============================================================================
// SortedContainer - Auto-Sorting Container
// =============================================================================

/// @brief Container that maintains sorted order
class SortedContainer : public Container {
public:
    using SortKeyFunc = std::function<int(const ItemInstance&)>;

    SortedContainer();
    SortedContainer(ContainerId id, std::size_t capacity);
    ~SortedContainer() override;

    ContainerType type() const override { return ContainerType::Sorted; }

    /// @brief Set sort key function
    void set_sort_key(SortKeyFunc key_func);

    /// @brief Set sort ascending/descending
    void set_sort_ascending(bool ascending) { m_ascending = ascending; }

    /// @brief Enable auto-sort on add
    void set_auto_sort(bool auto_sort) { m_auto_sort = auto_sort; }

    TransactionResult add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot = nullptr) override;
    void sort() override;

    // Preset sort keys
    static SortKeyFunc sort_by_name();
    static SortKeyFunc sort_by_rarity();
    static SortKeyFunc sort_by_category();
    static SortKeyFunc sort_by_value();
    static SortKeyFunc sort_by_weight();

private:
    SortKeyFunc m_sort_key;
    bool m_ascending{true};
    bool m_auto_sort{true};
};

// =============================================================================
// ContainerManager
// =============================================================================

/// @brief Manages multiple containers
class ContainerManager {
public:
    ContainerManager();
    ~ContainerManager();

    /// @brief Create a new basic container
    ContainerId create_container(const std::string& name, std::size_t capacity);

    /// @brief Create a grid container
    ContainerId create_grid_container(const std::string& name, std::uint32_t width, std::uint32_t height);

    /// @brief Create a weighted container
    ContainerId create_weighted_container(const std::string& name, std::size_t capacity, float weight_limit);

    /// @brief Register external container
    void register_container(std::unique_ptr<IContainer> container);

    /// @brief Get container by ID
    IContainer* get_container(ContainerId id);
    const IContainer* get_container(ContainerId id) const;

    /// @brief Remove container
    bool remove_container(ContainerId id);

    /// @brief Get all container IDs
    std::vector<ContainerId> all_containers() const;

    /// @brief Transfer items between containers
    TransactionResult transfer(ContainerId source, std::uint32_t source_slot,
                               ContainerId dest, std::uint32_t dest_slot,
                               std::uint32_t quantity);

    /// @brief Transfer all matching items
    std::uint32_t transfer_all(ContainerId source, ContainerId dest, ItemDefId def);

    /// @brief Set item database for all containers
    void set_item_database(ItemDatabase* db);

    /// @brief Clear all containers
    void clear_all();

private:
    std::unordered_map<ContainerId, std::unique_ptr<IContainer>> m_containers;
    ItemDatabase* m_item_db{nullptr};
    std::uint64_t m_next_id{1};
};

} // namespace void_inventory
