/// @file containers.cpp
/// @brief Container system implementation for void_inventory module

#include <void_engine/inventory/containers.hpp>

#include <algorithm>

namespace void_inventory {

// =============================================================================
// Container Implementation
// =============================================================================

Container::Container() = default;

Container::Container(ContainerId id, std::size_t capacity)
    : m_id(id)
    , m_slots(capacity) {
    for (std::size_t i = 0; i < capacity; ++i) {
        m_slots[i].index = static_cast<std::uint32_t>(i);
    }
}

Container::Container(ContainerId id, const std::string& name, std::size_t capacity)
    : m_id(id)
    , m_name(name)
    , m_slots(capacity) {
    for (std::size_t i = 0; i < capacity; ++i) {
        m_slots[i].index = static_cast<std::uint32_t>(i);
    }
}

Container::~Container() = default;

Container::Container(Container&&) noexcept = default;
Container& Container::operator=(Container&&) noexcept = default;

std::size_t Container::size() const {
    std::size_t count = 0;
    for (const auto& slot : m_slots) {
        if (!slot.empty()) {
            ++count;
        }
    }
    return count;
}

std::size_t Container::free_slots() const {
    return capacity() - size();
}

bool Container::empty() const {
    for (const auto& slot : m_slots) {
        if (!slot.empty()) {
            return false;
        }
    }
    return true;
}

bool Container::full() const {
    for (const auto& slot : m_slots) {
        if (slot.empty()) {
            return false;
        }
    }
    return true;
}

const SlotState* Container::get_slot(std::uint32_t index) const {
    if (index >= m_slots.size()) {
        return nullptr;
    }
    return &m_slots[index];
}

ItemInstanceId Container::get_item(std::uint32_t slot) const {
    if (slot >= m_slots.size()) {
        return ItemInstanceId{};
    }
    return m_slots[slot].item;
}

std::uint32_t Container::get_quantity(std::uint32_t slot) const {
    if (slot >= m_slots.size()) {
        return 0;
    }
    return m_slots[slot].quantity;
}

TransactionResult Container::add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot) {
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    // Get item instance for filtering
    const ItemInstance* inst = get_item_instance(item);
    if (inst && m_filter && !m_filter(*inst)) {
        return TransactionResult::PermissionDenied;
    }

    // Try to stack with existing items first
    if (inst && inst->def && inst->def->max_stack > 1) {
        for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
            auto& slot = m_slots[i];
            if (slot.item == item || (inst && slot.item)) {
                const ItemInstance* existing = get_item_instance(slot.item);
                if (existing && existing->def_id == inst->def_id) {
                    std::uint32_t space = inst->def->max_stack - slot.quantity;
                    if (space > 0) {
                        std::uint32_t to_add = std::min(space, quantity);
                        slot.quantity += to_add;
                        quantity -= to_add;

                        if (out_slot) *out_slot = i;
                        notify_change(i);

                        if (quantity == 0) {
                            return TransactionResult::Success;
                        }
                    }
                }
            }
        }
    }

    // Find empty slot
    auto empty_slot = find_empty_slot();
    if (!empty_slot) {
        return quantity > 0 ? TransactionResult::ContainerFull : TransactionResult::Success;
    }

    // Check slot filter
    if (inst && m_slot_filter && !m_slot_filter(*inst, *empty_slot)) {
        return TransactionResult::PermissionDenied;
    }

    m_slots[*empty_slot].item = item;
    m_slots[*empty_slot].quantity = quantity;
    if (out_slot) *out_slot = *empty_slot;

    notify_change(*empty_slot);
    return TransactionResult::Success;
}

TransactionResult Container::add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) {
    if (slot >= m_slots.size()) {
        return TransactionResult::InvalidSlot;
    }

    if (!item) {
        return TransactionResult::InvalidItem;
    }

    auto& target = m_slots[slot];

    // Check if slot is locked
    if (has_flag(target.flags, SlotFlags::Locked)) {
        return TransactionResult::ItemLocked;
    }

    const ItemInstance* inst = get_item_instance(item);
    if (inst && m_filter && !m_filter(*inst)) {
        return TransactionResult::PermissionDenied;
    }

    if (inst && m_slot_filter && !m_slot_filter(*inst, slot)) {
        return TransactionResult::PermissionDenied;
    }

    if (target.empty()) {
        target.item = item;
        target.quantity = quantity;
        notify_change(slot);
        return TransactionResult::Success;
    }

    // Try to stack
    const ItemInstance* existing = get_item_instance(target.item);
    if (!inst || !existing || inst->def_id != existing->def_id) {
        return TransactionResult::ItemNotStackable;
    }

    if (!inst->def || inst->def->max_stack <= 1) {
        return TransactionResult::ItemNotStackable;
    }

    std::uint32_t space = inst->def->max_stack - target.quantity;
    if (space == 0) {
        return TransactionResult::ContainerFull;
    }

    target.quantity += std::min(space, quantity);
    notify_change(slot);
    return TransactionResult::Success;
}

TransactionResult Container::remove(std::uint32_t slot, std::uint32_t quantity) {
    if (slot >= m_slots.size()) {
        return TransactionResult::InvalidSlot;
    }

    auto& target = m_slots[slot];
    if (target.empty()) {
        return TransactionResult::InvalidItem;
    }

    if (has_flag(target.flags, SlotFlags::Locked)) {
        return TransactionResult::ItemLocked;
    }

    if (quantity >= target.quantity) {
        target.item = ItemInstanceId{};
        target.quantity = 0;
    } else {
        target.quantity -= quantity;
    }

    notify_change(slot);
    return TransactionResult::Success;
}

TransactionResult Container::remove_item(ItemInstanceId item, std::uint32_t quantity) {
    auto slot = find_item(item);
    if (!slot) {
        return TransactionResult::InvalidItem;
    }
    return remove(*slot, quantity);
}

TransactionResult Container::move(std::uint32_t from_slot, std::uint32_t to_slot) {
    if (from_slot >= m_slots.size() || to_slot >= m_slots.size()) {
        return TransactionResult::InvalidSlot;
    }

    auto& from = m_slots[from_slot];
    auto& to = m_slots[to_slot];

    if (has_flag(from.flags, SlotFlags::Locked) || has_flag(to.flags, SlotFlags::Locked)) {
        return TransactionResult::ItemLocked;
    }

    if (from.empty()) {
        return TransactionResult::InvalidItem;
    }

    // Check slot filter for destination
    const ItemInstance* inst = get_item_instance(from.item);
    if (inst && m_slot_filter && !m_slot_filter(*inst, to_slot)) {
        return TransactionResult::PermissionDenied;
    }

    // Try to stack if same item type
    if (!to.empty()) {
        const ItemInstance* to_inst = get_item_instance(to.item);
        if (inst && to_inst && inst->def_id == to_inst->def_id) {
            if (inst->def && inst->def->max_stack > 1) {
                std::uint32_t space = inst->def->max_stack - to.quantity;
                std::uint32_t to_move = std::min(space, from.quantity);
                to.quantity += to_move;
                from.quantity -= to_move;
                if (from.quantity == 0) {
                    from.item = ItemInstanceId{};
                }
                notify_change(from_slot);
                notify_change(to_slot);
                return TransactionResult::Success;
            }
        }
        // Can't stack, swap instead
        return swap(from_slot, to_slot);
    }

    to.item = from.item;
    to.quantity = from.quantity;
    from.item = ItemInstanceId{};
    from.quantity = 0;

    notify_change(from_slot);
    notify_change(to_slot);
    return TransactionResult::Success;
}

TransactionResult Container::swap(std::uint32_t slot_a, std::uint32_t slot_b) {
    if (slot_a >= m_slots.size() || slot_b >= m_slots.size()) {
        return TransactionResult::InvalidSlot;
    }

    auto& a = m_slots[slot_a];
    auto& b = m_slots[slot_b];

    if (has_flag(a.flags, SlotFlags::Locked) || has_flag(b.flags, SlotFlags::Locked)) {
        return TransactionResult::ItemLocked;
    }

    // Check slot filters
    const ItemInstance* inst_a = get_item_instance(a.item);
    const ItemInstance* inst_b = get_item_instance(b.item);

    if (m_slot_filter) {
        if (inst_a && !m_slot_filter(*inst_a, slot_b)) {
            return TransactionResult::PermissionDenied;
        }
        if (inst_b && !m_slot_filter(*inst_b, slot_a)) {
            return TransactionResult::PermissionDenied;
        }
    }

    std::swap(a.item, b.item);
    std::swap(a.quantity, b.quantity);

    notify_change(slot_a);
    notify_change(slot_b);
    return TransactionResult::Success;
}

std::optional<std::uint32_t> Container::find_item(ItemInstanceId item) const {
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].item == item) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> Container::find_item_def(ItemDefId def) const {
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].empty()) {
            const ItemInstance* inst = get_item_instance(m_slots[i].item);
            if (inst && inst->def_id == def) {
                return i;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> Container::find_empty_slot() const {
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].empty() && !has_flag(m_slots[i].flags, SlotFlags::Locked)) {
            return i;
        }
    }
    return std::nullopt;
}

std::vector<std::uint32_t> Container::find_all(ItemDefId def) const {
    std::vector<std::uint32_t> result;
    for (std::uint32_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].empty()) {
            const ItemInstance* inst = get_item_instance(m_slots[i].item);
            if (inst && inst->def_id == def) {
                result.push_back(i);
            }
        }
    }
    return result;
}

std::uint32_t Container::count_item(ItemDefId def) const {
    std::uint32_t count = 0;
    for (const auto& slot : m_slots) {
        if (!slot.empty()) {
            const ItemInstance* inst = get_item_instance(slot.item);
            if (inst && inst->def_id == def) {
                count += slot.quantity;
            }
        }
    }
    return count;
}

bool Container::contains(ItemInstanceId item) const {
    return find_item(item).has_value();
}

bool Container::contains_def(ItemDefId def) const {
    return find_item_def(def).has_value();
}

void Container::clear() {
    for (auto& slot : m_slots) {
        slot.item = ItemInstanceId{};
        slot.quantity = 0;
    }
}

void Container::sort() {
    // Collect all items
    std::vector<std::pair<ItemInstanceId, std::uint32_t>> items;
    for (const auto& slot : m_slots) {
        if (!slot.empty()) {
            items.emplace_back(slot.item, slot.quantity);
        }
    }

    // Sort by definition ID
    std::sort(items.begin(), items.end(),
        [this](const auto& a, const auto& b) {
            const ItemInstance* inst_a = get_item_instance(a.first);
            const ItemInstance* inst_b = get_item_instance(b.first);
            if (!inst_a) return false;
            if (!inst_b) return true;
            return inst_a->def_id.value < inst_b->def_id.value;
        });

    // Clear and refill
    for (auto& slot : m_slots) {
        slot.item = ItemInstanceId{};
        slot.quantity = 0;
    }

    for (std::size_t i = 0; i < items.size() && i < m_slots.size(); ++i) {
        m_slots[i].item = items[i].first;
        m_slots[i].quantity = items[i].second;
    }
}

void Container::compact() {
    std::size_t write_idx = 0;
    for (std::size_t read_idx = 0; read_idx < m_slots.size(); ++read_idx) {
        if (!m_slots[read_idx].empty()) {
            if (write_idx != read_idx) {
                m_slots[write_idx] = m_slots[read_idx];
                m_slots[read_idx].item = ItemInstanceId{};
                m_slots[read_idx].quantity = 0;
            }
            ++write_idx;
        }
    }
}

void Container::resize(std::size_t new_capacity) {
    m_slots.resize(new_capacity);
    for (std::size_t i = 0; i < new_capacity; ++i) {
        m_slots[i].index = static_cast<std::uint32_t>(i);
    }
}

void Container::configure_slot(std::uint32_t slot, const SlotConfig& config) {
    if (slot < m_slots.size()) {
        m_slot_configs[slot] = config;
        m_slots[slot].flags = config.flags;
    }
}

const SlotConfig* Container::get_slot_config(std::uint32_t slot) const {
    auto it = m_slot_configs.find(slot);
    return it != m_slot_configs.end() ? &it->second : nullptr;
}

void Container::notify_change(std::uint32_t slot) {
    if (m_on_change) {
        m_on_change(m_id, slot);
    }
}

bool Container::passes_filter(const ItemInstance& item) const {
    return !m_filter || m_filter(item);
}

bool Container::passes_slot_filter(const ItemInstance& item, std::uint32_t slot) const {
    return !m_slot_filter || m_slot_filter(item, slot);
}

const ItemInstance* Container::get_item_instance(ItemInstanceId id) const {
    if (!m_item_db || !id) {
        return nullptr;
    }
    auto opt = m_item_db->retrieve(id);
    // Note: This is a workaround - in production, you'd want to cache these
    // or use a different pattern to avoid the optional copy
    static thread_local ItemInstance cached;
    if (opt) {
        cached = *opt;
        return &cached;
    }
    return nullptr;
}

// =============================================================================
// GridContainer Implementation
// =============================================================================

GridContainer::GridContainer() = default;

GridContainer::GridContainer(ContainerId id, std::uint32_t width, std::uint32_t height)
    : m_id(id)
    , m_width(width)
    , m_height(height)
    , m_grid(width * height) {
}

GridContainer::~GridContainer() = default;

std::size_t GridContainer::size() const {
    return m_items.size();
}

std::size_t GridContainer::free_slots() const {
    std::size_t free = 0;
    for (const auto& cell : m_grid) {
        if (!cell) {
            ++free;
        }
    }
    return free;
}

bool GridContainer::empty() const {
    return m_items.empty();
}

bool GridContainer::full() const {
    for (const auto& cell : m_grid) {
        if (!cell) {
            return false;
        }
    }
    return true;
}

const SlotState* GridContainer::get_slot(std::uint32_t index) const {
    // Grid container doesn't use traditional slots
    return nullptr;
}

ItemInstanceId GridContainer::get_item(std::uint32_t slot) const {
    if (slot >= m_grid.size()) {
        return ItemInstanceId{};
    }
    return m_grid[slot];
}

std::uint32_t GridContainer::get_quantity(std::uint32_t slot) const {
    ItemInstanceId item = get_item(slot);
    for (const auto& gi : m_items) {
        if (gi.item == item) {
            return gi.quantity;
        }
    }
    return 0;
}

TransactionResult GridContainer::add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot) {
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    const ItemInstance* inst = get_item_instance(item);
    if (!inst || !inst->def) {
        return TransactionResult::InvalidItem;
    }

    GridSize size = inst->def->grid_size;
    auto pos = find_free_position(size.width, size.height);
    if (!pos) {
        return TransactionResult::ContainerFull;
    }

    GridItem gi;
    gi.item = item;
    gi.quantity = quantity;
    gi.position = *pos;
    gi.size = size;

    mark_cells(*pos, size, item);
    m_items.push_back(gi);

    if (out_slot) {
        *out_slot = grid_to_slot(pos->x, pos->y);
    }

    return TransactionResult::Success;
}

TransactionResult GridContainer::add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) {
    auto pos = slot_to_grid(slot);
    return add_at(item, pos.x, pos.y);
}

TransactionResult GridContainer::add_at(ItemInstanceId item, std::uint32_t x, std::uint32_t y) {
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    const ItemInstance* inst = get_item_instance(item);
    if (!inst || !inst->def) {
        return TransactionResult::InvalidItem;
    }

    GridSize size = inst->def->grid_size;
    if (!is_area_free(x, y, size.width, size.height)) {
        return TransactionResult::ContainerFull;
    }

    GridItem gi;
    gi.item = item;
    gi.quantity = inst->quantity;
    gi.position = {x, y};
    gi.size = size;

    mark_cells({x, y}, size, item);
    m_items.push_back(gi);

    return TransactionResult::Success;
}

TransactionResult GridContainer::remove(std::uint32_t slot, std::uint32_t quantity) {
    ItemInstanceId item = get_item(slot);
    if (!item) {
        return TransactionResult::InvalidItem;
    }
    return remove_item(item, quantity);
}

TransactionResult GridContainer::remove_item(ItemInstanceId item, std::uint32_t quantity) {
    for (auto it = m_items.begin(); it != m_items.end(); ++it) {
        if (it->item == item) {
            if (quantity >= it->quantity) {
                clear_cells(it->position, it->size);
                m_items.erase(it);
            } else {
                it->quantity -= quantity;
            }
            return TransactionResult::Success;
        }
    }
    return TransactionResult::InvalidItem;
}

TransactionResult GridContainer::move(std::uint32_t from_slot, std::uint32_t to_slot) {
    auto from_pos = slot_to_grid(from_slot);
    auto to_pos = slot_to_grid(to_slot);

    ItemInstanceId item = get_at(from_pos.x, from_pos.y);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    // Find the item
    GridItem* gi = nullptr;
    for (auto& i : m_items) {
        if (i.item == item) {
            gi = &i;
            break;
        }
    }

    if (!gi) {
        return TransactionResult::InvalidItem;
    }

    // Clear old position
    clear_cells(gi->position, gi->size);

    // Check new position
    if (!is_area_free(to_pos.x, to_pos.y, gi->size.width, gi->size.height)) {
        // Restore old position
        mark_cells(gi->position, gi->size, item);
        return TransactionResult::ContainerFull;
    }

    // Move to new position
    gi->position = to_pos;
    mark_cells(to_pos, gi->size, item);

    return TransactionResult::Success;
}

TransactionResult GridContainer::swap(std::uint32_t slot_a, std::uint32_t slot_b) {
    // Grid swap is complex - just use move for now
    return TransactionResult::Failed;
}

std::optional<std::uint32_t> GridContainer::find_item(ItemInstanceId item) const {
    for (const auto& gi : m_items) {
        if (gi.item == item) {
            return grid_to_slot(gi.position.x, gi.position.y);
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> GridContainer::find_item_def(ItemDefId def) const {
    for (const auto& gi : m_items) {
        const ItemInstance* inst = get_item_instance(gi.item);
        if (inst && inst->def_id == def) {
            return grid_to_slot(gi.position.x, gi.position.y);
        }
    }
    return std::nullopt;
}

std::optional<std::uint32_t> GridContainer::find_empty_slot() const {
    auto pos = find_free_position(1, 1);
    if (pos) {
        return grid_to_slot(pos->x, pos->y);
    }
    return std::nullopt;
}

std::vector<std::uint32_t> GridContainer::find_all(ItemDefId def) const {
    std::vector<std::uint32_t> result;
    for (const auto& gi : m_items) {
        const ItemInstance* inst = get_item_instance(gi.item);
        if (inst && inst->def_id == def) {
            result.push_back(grid_to_slot(gi.position.x, gi.position.y));
        }
    }
    return result;
}

std::uint32_t GridContainer::count_item(ItemDefId def) const {
    std::uint32_t count = 0;
    for (const auto& gi : m_items) {
        const ItemInstance* inst = get_item_instance(gi.item);
        if (inst && inst->def_id == def) {
            count += gi.quantity;
        }
    }
    return count;
}

bool GridContainer::contains(ItemInstanceId item) const {
    return find_item(item).has_value();
}

bool GridContainer::contains_def(ItemDefId def) const {
    return find_item_def(def).has_value();
}

void GridContainer::clear() {
    m_items.clear();
    std::fill(m_grid.begin(), m_grid.end(), ItemInstanceId{});
}

void GridContainer::sort() {
    // Grid containers don't typically sort
}

void GridContainer::compact() {
    // Move all items to top-left
    std::vector<GridItem> items = std::move(m_items);
    std::fill(m_grid.begin(), m_grid.end(), ItemInstanceId{});
    m_items.clear();

    for (auto& gi : items) {
        auto pos = find_free_position(gi.size.width, gi.size.height);
        if (pos) {
            gi.position = *pos;
            mark_cells(*pos, gi.size, gi.item);
            m_items.push_back(std::move(gi));
        }
    }
}

bool GridContainer::is_area_free(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) const {
    if (x + w > m_width || y + h > m_height) {
        return false;
    }

    for (std::uint32_t cy = y; cy < y + h; ++cy) {
        for (std::uint32_t cx = x; cx < x + w; ++cx) {
            if (m_grid[cy * m_width + cx]) {
                return false;
            }
        }
    }

    return true;
}

std::optional<GridPosition> GridContainer::find_free_position(std::uint32_t w, std::uint32_t h) const {
    for (std::uint32_t y = 0; y <= m_height - h; ++y) {
        for (std::uint32_t x = 0; x <= m_width - w; ++x) {
            if (is_area_free(x, y, w, h)) {
                return GridPosition{x, y};
            }
        }
    }
    return std::nullopt;
}

ItemInstanceId GridContainer::get_at(std::uint32_t x, std::uint32_t y) const {
    if (x >= m_width || y >= m_height) {
        return ItemInstanceId{};
    }
    return m_grid[y * m_width + x];
}

void GridContainer::mark_cells(const GridPosition& pos, const GridSize& size, ItemInstanceId item) {
    for (std::uint32_t y = pos.y; y < pos.y + size.height; ++y) {
        for (std::uint32_t x = pos.x; x < pos.x + size.width; ++x) {
            m_grid[y * m_width + x] = item;
        }
    }
}

void GridContainer::clear_cells(const GridPosition& pos, const GridSize& size) {
    for (std::uint32_t y = pos.y; y < pos.y + size.height; ++y) {
        for (std::uint32_t x = pos.x; x < pos.x + size.width; ++x) {
            m_grid[y * m_width + x] = ItemInstanceId{};
        }
    }
}

const ItemInstance* GridContainer::get_item_instance(ItemInstanceId id) const {
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
// WeightedContainer Implementation
// =============================================================================

WeightedContainer::WeightedContainer() = default;

WeightedContainer::WeightedContainer(ContainerId id, std::size_t capacity, float weight_limit)
    : Container(id, capacity)
    , m_weight_limit(weight_limit) {
}

WeightedContainer::~WeightedContainer() = default;

TransactionResult WeightedContainer::add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot) {
    const ItemInstance* inst = get_item_instance(item);
    if (inst && inst->def) {
        float item_weight = inst->def->weight * quantity;
        if (!can_fit_weight(item_weight, 1)) {
            return TransactionResult::ContainerFull;
        }
        auto result = Container::add(item, quantity, out_slot);
        if (result == TransactionResult::Success) {
            m_current_weight += item_weight;
        }
        return result;
    }
    return Container::add(item, quantity, out_slot);
}

TransactionResult WeightedContainer::add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) {
    const ItemInstance* inst = get_item_instance(item);
    if (inst && inst->def) {
        float item_weight = inst->def->weight * quantity;
        if (!can_fit_weight(item_weight, 1)) {
            return TransactionResult::ContainerFull;
        }
        auto result = Container::add_to_slot(slot, item, quantity);
        if (result == TransactionResult::Success) {
            m_current_weight += item_weight;
        }
        return result;
    }
    return Container::add_to_slot(slot, item, quantity);
}

bool WeightedContainer::can_fit_weight(float item_weight, std::uint32_t /*quantity*/) const {
    return m_current_weight + item_weight <= m_weight_limit;
}

void WeightedContainer::recalculate_weight() {
    m_current_weight = 0;
    for (std::uint32_t i = 0; i < capacity(); ++i) {
        const SlotState* slot = get_slot(i);
        if (slot && !slot->empty()) {
            const ItemInstance* inst = get_item_instance(slot->item);
            if (inst && inst->def) {
                m_current_weight += inst->def->weight * slot->quantity;
            }
        }
    }
}

// =============================================================================
// FilteredContainer Implementation
// =============================================================================

FilteredContainer::FilteredContainer() = default;

FilteredContainer::FilteredContainer(ContainerId id, std::size_t capacity)
    : Container(id, capacity) {
}

FilteredContainer::~FilteredContainer() = default;

void FilteredContainer::set_allowed_categories(const std::vector<ItemCategory>& categories) {
    m_allowed_categories = categories;
}

void FilteredContainer::set_allowed_items(const std::vector<ItemDefId>& items) {
    m_allowed_items = items;
}

void FilteredContainer::set_required_tags(const std::vector<std::string>& tags) {
    m_required_tags = tags;
}

bool FilteredContainer::is_allowed(const ItemInstance& item) const {
    if (!item.def) {
        return false;
    }

    // Check categories
    if (!m_allowed_categories.empty()) {
        bool found = false;
        for (auto cat : m_allowed_categories) {
            if (cat == item.def->category) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Check specific items
    if (!m_allowed_items.empty()) {
        bool found = false;
        for (auto id : m_allowed_items) {
            if (id == item.def_id) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    // Check tags
    for (const auto& tag : m_required_tags) {
        if (!item.def->has_tag(tag)) {
            return false;
        }
    }

    return true;
}

TransactionResult FilteredContainer::add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot) {
    const ItemInstance* inst = get_item_instance(item);
    if (inst && !is_allowed(*inst)) {
        return TransactionResult::PermissionDenied;
    }
    return Container::add(item, quantity, out_slot);
}

TransactionResult FilteredContainer::add_to_slot(std::uint32_t slot, ItemInstanceId item, std::uint32_t quantity) {
    const ItemInstance* inst = get_item_instance(item);
    if (inst && !is_allowed(*inst)) {
        return TransactionResult::PermissionDenied;
    }
    return Container::add_to_slot(slot, item, quantity);
}

// =============================================================================
// SortedContainer Implementation
// =============================================================================

SortedContainer::SortedContainer() = default;

SortedContainer::SortedContainer(ContainerId id, std::size_t capacity)
    : Container(id, capacity) {
}

SortedContainer::~SortedContainer() = default;

void SortedContainer::set_sort_key(SortKeyFunc key_func) {
    m_sort_key = std::move(key_func);
}

TransactionResult SortedContainer::add(ItemInstanceId item, std::uint32_t quantity, std::uint32_t* out_slot) {
    auto result = Container::add(item, quantity, out_slot);
    if (result == TransactionResult::Success && m_auto_sort) {
        sort();
    }
    return result;
}

void SortedContainer::sort() {
    if (!m_sort_key) {
        Container::sort();
        return;
    }

    // Collect items with sort keys
    std::vector<std::tuple<ItemInstanceId, std::uint32_t, int>> items;
    for (std::uint32_t i = 0; i < capacity(); ++i) {
        const SlotState* slot = get_slot(i);
        if (slot && !slot->empty()) {
            const ItemInstance* inst = get_item_instance(slot->item);
            int key = inst ? m_sort_key(*inst) : 0;
            items.emplace_back(slot->item, slot->quantity, key);
        }
    }

    // Sort
    std::sort(items.begin(), items.end(),
        [this](const auto& a, const auto& b) {
            return m_ascending ? std::get<2>(a) < std::get<2>(b)
                               : std::get<2>(a) > std::get<2>(b);
        });

    // Clear and refill
    clear();
    for (std::size_t i = 0; i < items.size(); ++i) {
        // Direct slot access to bypass auto-sort
        auto* slot = const_cast<SlotState*>(get_slot(static_cast<std::uint32_t>(i)));
        if (slot) {
            slot->item = std::get<0>(items[i]);
            slot->quantity = std::get<1>(items[i]);
        }
    }
}

SortedContainer::SortKeyFunc SortedContainer::sort_by_name() {
    return [](const ItemInstance& item) -> int {
        return item.def ? static_cast<int>(std::hash<std::string>{}(item.def->display_name) % 10000) : 0;
    };
}

SortedContainer::SortKeyFunc SortedContainer::sort_by_rarity() {
    return [](const ItemInstance& item) -> int {
        return static_cast<int>(item.rarity);
    };
}

SortedContainer::SortKeyFunc SortedContainer::sort_by_category() {
    return [](const ItemInstance& item) -> int {
        return item.def ? static_cast<int>(item.def->category) : 0;
    };
}

SortedContainer::SortKeyFunc SortedContainer::sort_by_value() {
    return [](const ItemInstance& item) -> int {
        return static_cast<int>(item.calculate_value());
    };
}

SortedContainer::SortKeyFunc SortedContainer::sort_by_weight() {
    return [](const ItemInstance& item) -> int {
        return item.def ? static_cast<int>(item.def->weight * 100) : 0;
    };
}

// =============================================================================
// ContainerManager Implementation
// =============================================================================

ContainerManager::ContainerManager() = default;
ContainerManager::~ContainerManager() = default;

ContainerId ContainerManager::create_container(const std::string& name, std::size_t capacity) {
    ContainerId id{m_next_id++};
    auto container = std::make_unique<Container>(id, name, capacity);
    if (m_item_db) {
        container->set_item_database(m_item_db);
    }
    m_containers[id] = std::move(container);
    return id;
}

ContainerId ContainerManager::create_grid_container(const std::string& name, std::uint32_t width, std::uint32_t height) {
    ContainerId id{m_next_id++};
    auto container = std::make_unique<GridContainer>(id, width, height);
    if (m_item_db) {
        container->set_item_database(m_item_db);
    }
    m_containers[id] = std::move(container);
    return id;
}

ContainerId ContainerManager::create_weighted_container(const std::string& name, std::size_t capacity, float weight_limit) {
    ContainerId id{m_next_id++};
    auto container = std::make_unique<WeightedContainer>(id, capacity, weight_limit);
    if (m_item_db) {
        container->set_item_database(m_item_db);
    }
    m_containers[id] = std::move(container);
    return id;
}

void ContainerManager::register_container(std::unique_ptr<IContainer> container) {
    if (container) {
        m_containers[container->id()] = std::move(container);
    }
}

IContainer* ContainerManager::get_container(ContainerId id) {
    auto it = m_containers.find(id);
    return it != m_containers.end() ? it->second.get() : nullptr;
}

const IContainer* ContainerManager::get_container(ContainerId id) const {
    auto it = m_containers.find(id);
    return it != m_containers.end() ? it->second.get() : nullptr;
}

bool ContainerManager::remove_container(ContainerId id) {
    return m_containers.erase(id) > 0;
}

std::vector<ContainerId> ContainerManager::all_containers() const {
    std::vector<ContainerId> result;
    result.reserve(m_containers.size());
    for (const auto& [id, container] : m_containers) {
        result.push_back(id);
    }
    return result;
}

TransactionResult ContainerManager::transfer(ContainerId source, std::uint32_t source_slot,
                                             ContainerId dest, std::uint32_t dest_slot,
                                             std::uint32_t quantity) {
    IContainer* src = get_container(source);
    IContainer* dst = get_container(dest);

    if (!src || !dst) {
        return TransactionResult::InvalidSlot;
    }

    ItemInstanceId item = src->get_item(source_slot);
    if (!item) {
        return TransactionResult::InvalidItem;
    }

    auto result = dst->add_to_slot(dest_slot, item, quantity);
    if (result == TransactionResult::Success) {
        src->remove(source_slot, quantity);
    }

    return result;
}

std::uint32_t ContainerManager::transfer_all(ContainerId source, ContainerId dest, ItemDefId def) {
    IContainer* src = get_container(source);
    IContainer* dst = get_container(dest);

    if (!src || !dst) {
        return 0;
    }

    std::uint32_t transferred = 0;
    auto slots = src->find_all(def);

    for (auto slot : slots) {
        ItemInstanceId item = src->get_item(slot);
        std::uint32_t qty = src->get_quantity(slot);

        if (dst->add(item, qty, nullptr) == TransactionResult::Success) {
            src->remove(slot, qty);
            transferred += qty;
        }
    }

    return transferred;
}

void ContainerManager::set_item_database(ItemDatabase* db) {
    m_item_db = db;
    for (auto& [id, container] : m_containers) {
        if (auto* c = dynamic_cast<Container*>(container.get())) {
            c->set_item_database(db);
        } else if (auto* g = dynamic_cast<GridContainer*>(container.get())) {
            g->set_item_database(db);
        }
    }
}

void ContainerManager::clear_all() {
    for (auto& [id, container] : m_containers) {
        container->clear();
    }
}

} // namespace void_inventory
