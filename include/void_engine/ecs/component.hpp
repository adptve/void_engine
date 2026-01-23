#pragma once

/// @file component.hpp
/// @brief Component types and storage for void_ecs
///
/// Components are stored as type-erased bytes with metadata for size,
/// alignment, and destruction. This enables runtime component registration
/// while maintaining type safety where needed.

#include "fwd.hpp"
#include <vector>
#include <string>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <memory>
#include <cstring>
#include <cassert>
#include <functional>
#include <limits>

namespace void_ecs {

// =============================================================================
// ComponentId
// =============================================================================

/// Unique identifier for a component type
struct ComponentId {
    std::uint32_t id;

    /// Invalid component ID constant
    static constexpr std::uint32_t INVALID_ID = std::numeric_limits<std::uint32_t>::max();

    /// Create from raw ID
    constexpr explicit ComponentId(std::uint32_t i = INVALID_ID) noexcept : id(i) {}

    /// Get raw ID value
    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return id; }

    /// Check if valid
    [[nodiscard]] constexpr bool is_valid() const noexcept { return id != INVALID_ID; }

    /// Invalid component ID factory
    [[nodiscard]] static constexpr ComponentId invalid() noexcept {
        return ComponentId{INVALID_ID};
    }

    // Comparison operators
    [[nodiscard]] constexpr bool operator==(const ComponentId& other) const noexcept {
        return id == other.id;
    }
    [[nodiscard]] constexpr bool operator!=(const ComponentId& other) const noexcept {
        return id != other.id;
    }
    [[nodiscard]] constexpr bool operator<(const ComponentId& other) const noexcept {
        return id < other.id;
    }
    [[nodiscard]] constexpr bool operator<=(const ComponentId& other) const noexcept {
        return id <= other.id;
    }
    [[nodiscard]] constexpr bool operator>(const ComponentId& other) const noexcept {
        return id > other.id;
    }
    [[nodiscard]] constexpr bool operator>=(const ComponentId& other) const noexcept {
        return id >= other.id;
    }
};

} // namespace void_ecs

template<>
struct std::hash<void_ecs::ComponentId> {
    [[nodiscard]] std::size_t operator()(const void_ecs::ComponentId& id) const noexcept {
        return std::hash<std::uint32_t>{}(id.id);
    }
};

namespace void_ecs {

// =============================================================================
// ComponentInfo
// =============================================================================

/// Metadata for a component type
struct ComponentInfo {
    ComponentId id{ComponentId::INVALID_ID};
    std::string name;
    std::size_t size{0};
    std::size_t align{0};
    std::type_index type_id{typeid(void)};

    /// Function to destruct a component at the given address
    std::function<void(void*)> drop_fn;

    /// Function to move-construct a component from src to dst
    std::function<void(void*, void*)> move_fn;

    /// Function to copy-construct a component from src to dst (optional)
    std::function<void(const void*, void*)> clone_fn;

    /// Create info for a typed component
    template<typename T>
    [[nodiscard]] static ComponentInfo of() {
        ComponentInfo info;
        info.name = typeid(T).name();
        info.size = sizeof(T);
        info.align = alignof(T);
        info.type_id = std::type_index(typeid(T));

        // Drop function
        info.drop_fn = [](void* ptr) {
            static_cast<T*>(ptr)->~T();
        };

        // Move function
        info.move_fn = [](void* src, void* dst) {
            new (dst) T(std::move(*static_cast<T*>(src)));
        };

        return info;
    }

    /// Create info for a cloneable typed component
    template<typename T>
    [[nodiscard]] static ComponentInfo of_cloneable() {
        ComponentInfo info = of<T>();

        // Clone function
        info.clone_fn = [](const void* src, void* dst) {
            new (dst) T(*static_cast<const T*>(src));
        };

        return info;
    }

    /// Check if this component type is cloneable
    [[nodiscard]] bool is_cloneable() const noexcept {
        return clone_fn != nullptr;
    }
};

// =============================================================================
// ComponentRegistry
// =============================================================================

/// Registry of all component types
///
/// Maps type information to component IDs and stores metadata.
class ComponentRegistry {
public:
    using size_type = std::size_t;

private:
    std::vector<ComponentInfo> components_;
    std::unordered_map<std::type_index, ComponentId> type_map_;
    std::unordered_map<std::string, ComponentId> name_map_;

public:
    // =========================================================================
    // Registration
    // =========================================================================

    /// Register a component type
    /// @return Component ID (existing ID if already registered)
    template<typename T>
    ComponentId register_component() {
        std::type_index type_idx = std::type_index(typeid(T));

        // Check if already registered
        auto it = type_map_.find(type_idx);
        if (it != type_map_.end()) {
            return it->second;
        }

        // Create new component
        ComponentInfo info = ComponentInfo::of<T>();
        return register_info(std::move(info), type_idx);
    }

    /// Register a cloneable component type
    template<typename T>
    ComponentId register_cloneable() {
        std::type_index type_idx = std::type_index(typeid(T));

        auto it = type_map_.find(type_idx);
        if (it != type_map_.end()) {
            return it->second;
        }

        ComponentInfo info = ComponentInfo::of_cloneable<T>();
        return register_info(std::move(info), type_idx);
    }

    /// Register a dynamically-defined component
    ComponentId register_dynamic(ComponentInfo info) {
        ComponentId id{static_cast<std::uint32_t>(components_.size())};
        info.id = id;

        name_map_[info.name] = id;
        components_.push_back(std::move(info));

        return id;
    }

    // =========================================================================
    // Lookup
    // =========================================================================

    /// Get component ID by type
    template<typename T>
    [[nodiscard]] std::optional<ComponentId> get_id() const {
        std::type_index type_idx = std::type_index(typeid(T));
        auto it = type_map_.find(type_idx);
        if (it != type_map_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Get component ID by name
    [[nodiscard]] std::optional<ComponentId> get_id_by_name(const std::string& name) const {
        auto it = name_map_.find(name);
        if (it != name_map_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    /// Get component info by ID
    [[nodiscard]] const ComponentInfo* get_info(ComponentId id) const noexcept {
        if (id.id >= components_.size()) {
            return nullptr;
        }
        return &components_[id.id];
    }

    /// Get component info by ID (mutable)
    [[nodiscard]] ComponentInfo* get_info(ComponentId id) noexcept {
        if (id.id >= components_.size()) {
            return nullptr;
        }
        return &components_[id.id];
    }

    // =========================================================================
    // Iteration
    // =========================================================================

    /// Number of registered components
    [[nodiscard]] size_type size() const noexcept {
        return components_.size();
    }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept {
        return components_.empty();
    }

    /// Iterate over all component infos
    [[nodiscard]] const std::vector<ComponentInfo>& all() const noexcept {
        return components_;
    }

    /// Begin iterator
    [[nodiscard]] auto begin() const noexcept { return components_.begin(); }

    /// End iterator
    [[nodiscard]] auto end() const noexcept { return components_.end(); }

private:
    ComponentId register_info(ComponentInfo info, std::type_index type_idx) {
        ComponentId id{static_cast<std::uint32_t>(components_.size())};
        info.id = id;

        type_map_[type_idx] = id;
        name_map_[info.name] = id;
        components_.push_back(std::move(info));

        return id;
    }
};

// =============================================================================
// ComponentStorage
// =============================================================================

/// Type-erased storage for components of a single type
///
/// Stores components as raw bytes with metadata for proper destruction.
class ComponentStorage {
public:
    using size_type = std::size_t;

private:
    ComponentInfo info_;
    std::vector<std::byte> data_;
    size_type len_{0};

public:
    // =========================================================================
    // Constructors
    // =========================================================================

    /// Create storage for a component type
    explicit ComponentStorage(ComponentInfo info)
        : info_(std::move(info)) {}

    /// Create storage with pre-allocated capacity
    ComponentStorage(ComponentInfo info, size_type capacity)
        : info_(std::move(info))
    {
        reserve(capacity);
    }

    /// Destructor - drops all stored components
    ~ComponentStorage() {
        clear();
    }

    // Non-copyable due to type-erased storage
    ComponentStorage(const ComponentStorage&) = delete;
    ComponentStorage& operator=(const ComponentStorage&) = delete;

    // Movable
    ComponentStorage(ComponentStorage&& other) noexcept
        : info_(std::move(other.info_))
        , data_(std::move(other.data_))
        , len_(other.len_)
    {
        other.len_ = 0;
    }

    ComponentStorage& operator=(ComponentStorage&& other) noexcept {
        if (this != &other) {
            clear();
            info_ = std::move(other.info_);
            data_ = std::move(other.data_);
            len_ = other.len_;
            other.len_ = 0;
        }
        return *this;
    }

    // =========================================================================
    // Properties
    // =========================================================================

    /// Get component info
    [[nodiscard]] const ComponentInfo& info() const noexcept { return info_; }

    /// Number of stored components
    [[nodiscard]] size_type size() const noexcept { return len_; }

    /// Alias for size()
    [[nodiscard]] size_type len() const noexcept { return len_; }

    /// Check if empty
    [[nodiscard]] bool empty() const noexcept { return len_ == 0; }

    /// Current capacity in components
    [[nodiscard]] size_type capacity() const noexcept {
        return info_.size > 0 ? data_.capacity() / info_.size : 0;
    }

    // =========================================================================
    // Capacity Management
    // =========================================================================

    /// Reserve capacity for additional components
    void reserve(size_type additional) {
        data_.reserve(data_.size() + additional * info_.size);
    }

    // =========================================================================
    // Typed Operations
    // =========================================================================

    /// Push a typed component
    template<typename T>
    void push(T&& value) {
        assert(info_.type_id == std::type_index(typeid(T)));

        // Ensure capacity
        size_type offset = len_ * info_.size;
        data_.resize(offset + info_.size);

        // Construct in place
        new (data_.data() + offset) T(std::forward<T>(value));
        ++len_;
    }

    /// Get typed component at index
    template<typename T>
    [[nodiscard]] const T& get(size_type index) const {
        assert(info_.type_id == std::type_index(typeid(T)));
        assert(index < len_);

        return *reinterpret_cast<const T*>(data_.data() + index * info_.size);
    }

    /// Get mutable typed component at index
    template<typename T>
    [[nodiscard]] T& get(size_type index) {
        assert(info_.type_id == std::type_index(typeid(T)));
        assert(index < len_);

        return *reinterpret_cast<T*>(data_.data() + index * info_.size);
    }

    /// Get typed slice
    template<typename T>
    [[nodiscard]] const T* as_slice() const {
        assert(info_.type_id == std::type_index(typeid(T)));
        return reinterpret_cast<const T*>(data_.data());
    }

    /// Get mutable typed slice
    template<typename T>
    [[nodiscard]] T* as_mut_slice() {
        assert(info_.type_id == std::type_index(typeid(T)));
        return reinterpret_cast<T*>(data_.data());
    }

    // =========================================================================
    // Raw Operations
    // =========================================================================

    /// Get raw pointer to component at index
    [[nodiscard]] void* get_raw(size_type index) noexcept {
        if (index >= len_) return nullptr;
        return data_.data() + index * info_.size;
    }

    /// Get const raw pointer to component at index
    [[nodiscard]] const void* get_raw(size_type index) const noexcept {
        if (index >= len_) return nullptr;
        return data_.data() + index * info_.size;
    }

    /// Push raw component data (caller must ensure correct type)
    void push_raw(const void* src) {
        size_type offset = len_ * info_.size;
        data_.resize(offset + info_.size);

        // Move-construct from source
        info_.move_fn(const_cast<void*>(src), data_.data() + offset);
        ++len_;
    }

    /// Copy raw bytes without construction (for archetype moves)
    void push_raw_bytes(const void* src) {
        size_type offset = len_ * info_.size;
        data_.resize(offset + info_.size);
        std::memcpy(data_.data() + offset, src, info_.size);
        ++len_;
    }

    /// Swap-remove component at index
    /// @return true if removal happened
    bool swap_remove(size_type index) {
        if (index >= len_) return false;

        void* to_remove = get_raw(index);

        // Call destructor
        if (info_.drop_fn) {
            info_.drop_fn(to_remove);
        }

        // If not last, swap with last
        if (index < len_ - 1) {
            void* last = get_raw(len_ - 1);
            std::memcpy(to_remove, last, info_.size);
        }

        // Shrink
        data_.resize((len_ - 1) * info_.size);
        --len_;
        return true;
    }

    /// Swap-remove without calling destructor (for moves between archetypes)
    bool swap_remove_no_drop(size_type index) {
        if (index >= len_) return false;

        void* to_remove = get_raw(index);

        // If not last, swap with last
        if (index < len_ - 1) {
            void* last = get_raw(len_ - 1);
            std::memcpy(to_remove, last, info_.size);
        }

        data_.resize((len_ - 1) * info_.size);
        --len_;
        return true;
    }

    /// Clear all components (calls destructors)
    void clear() {
        if (info_.drop_fn) {
            for (size_type i = 0; i < len_; ++i) {
                info_.drop_fn(get_raw(i));
            }
        }
        data_.clear();
        len_ = 0;
    }
};

// =============================================================================
// Component Concept (for type constraints)
// =============================================================================

/// Concept for valid component types
template<typename T>
concept Component = std::is_object_v<T> && !std::is_pointer_v<T>;

} // namespace void_ecs
