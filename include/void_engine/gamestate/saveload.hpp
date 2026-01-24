/// @file saveload.hpp
/// @brief Save/Load system for void_gamestate module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace void_gamestate {

// =============================================================================
// ISaveable Interface
// =============================================================================

/// @brief Interface for objects that can be saved/loaded
class ISaveable {
public:
    virtual ~ISaveable() = default;

    /// @brief Get unique identifier for this saveable
    virtual std::string saveable_id() const = 0;

    /// @brief Get save data version
    virtual std::uint32_t save_version() const = 0;

    /// @brief Serialize state to bytes
    virtual std::vector<std::uint8_t> serialize() const = 0;

    /// @brief Deserialize state from bytes
    virtual bool deserialize(const std::vector<std::uint8_t>& data, std::uint32_t version) = 0;

    /// @brief Called before save
    virtual void on_before_save() {}

    /// @brief Called after save
    virtual void on_after_save(SaveResult result) {}

    /// @brief Called before load
    virtual void on_before_load() {}

    /// @brief Called after load
    virtual void on_after_load(LoadResult result) {}
};

// =============================================================================
// SaveSerializer
// =============================================================================

/// @brief Handles serialization of save data
class SaveSerializer {
public:
    SaveSerializer();
    ~SaveSerializer();

    // Configuration
    void set_compression(bool enabled) { m_compress = enabled; }
    void set_encryption(bool enabled, const std::string& key = "") {
        m_encrypt = enabled;
        m_encryption_key = key;
    }

    // Serialization
    std::vector<std::uint8_t> serialize(const SaveData& data) const;
    SaveData deserialize(const std::vector<std::uint8_t>& bytes) const;

    // Individual components
    std::vector<std::uint8_t> serialize_metadata(const SaveMetadata& metadata) const;
    SaveMetadata deserialize_metadata(const std::vector<std::uint8_t>& bytes) const;

    // Checksum
    std::uint32_t calculate_checksum(const std::vector<std::uint8_t>& data) const;
    bool verify_checksum(const std::vector<std::uint8_t>& data, std::uint32_t checksum) const;

    // Compression
    std::vector<std::uint8_t> compress(const std::vector<std::uint8_t>& data) const;
    std::vector<std::uint8_t> decompress(const std::vector<std::uint8_t>& data) const;

    // Encryption
    std::vector<std::uint8_t> encrypt(const std::vector<std::uint8_t>& data) const;
    std::vector<std::uint8_t> decrypt(const std::vector<std::uint8_t>& data) const;

private:
    bool m_compress{true};
    bool m_encrypt{false};
    std::string m_encryption_key;
};

// =============================================================================
// SaveManager
// =============================================================================

/// @brief Manages save slots and save/load operations
class SaveManager {
public:
    SaveManager();
    explicit SaveManager(const GameStateConfig& config);
    ~SaveManager();

    // Configuration
    void set_save_directory(const std::filesystem::path& path);
    const std::filesystem::path& save_directory() const { return m_save_dir; }
    void set_max_slots(std::uint32_t count) { m_max_slots = count; }
    std::uint32_t max_slots() const { return m_max_slots; }

    // Slot management
    std::vector<SaveSlot> get_all_slots() const;
    SaveSlot get_slot(SaveSlotId slot) const;
    SaveSlotId get_empty_slot() const;
    SaveSlotId get_latest_slot() const;
    bool is_slot_empty(SaveSlotId slot) const;
    bool delete_slot(SaveSlotId slot);

    // Saveable registration
    void register_saveable(ISaveable* saveable);
    void unregister_saveable(ISaveable* saveable);
    void unregister_saveable(const std::string& id);

    // Save operations
    SaveResult save(SaveSlotId slot, const std::string& name = "", SaveType type = SaveType::Manual);
    SaveResult save_async(SaveSlotId slot, const std::string& name = "", SaveType type = SaveType::Manual);
    SaveResult quick_save();

    // Load operations
    LoadResult load(SaveSlotId slot);
    LoadResult load_async(SaveSlotId slot);
    LoadResult quick_load();

    // Metadata only
    SaveMetadata get_metadata(SaveSlotId slot) const;
    std::vector<SaveMetadata> get_all_metadata() const;

    // Import/Export
    SaveResult export_save(SaveSlotId slot, const std::filesystem::path& path) const;
    LoadResult import_save(const std::filesystem::path& path, SaveSlotId slot);

    // Validation
    bool validate_save(SaveSlotId slot) const;
    bool validate_save_file(const std::filesystem::path& path) const;
    bool is_compatible(const SaveMetadata& metadata) const;

    // Callbacks
    void set_on_save(SaveCallback callback) { m_on_save = std::move(callback); }
    void set_on_load(LoadCallback callback) { m_on_load = std::move(callback); }

    // State
    bool is_saving() const { return m_is_saving; }
    bool is_loading() const { return m_is_loading; }
    float progress() const { return m_progress; }

    // Utility
    void refresh_slots();
    std::uint64_t get_total_save_size() const;
    void set_game_version(const std::string& version) { m_game_version = version; }
    void set_current_level(const std::string& level) { m_current_level = level; }
    void set_play_time(double time) { m_play_time = time; }

    // Serializer access (for external state capture)
    const SaveSerializer& serializer() const { return m_serializer; }

    // Saveable access (for checkpoint system)
    const std::vector<ISaveable*>& saveables() const { return m_saveables; }

    // Gather and apply for external use
    SaveData gather_data(const std::string& name, SaveType type) const { return gather_save_data(name, type); }
    void apply_data(const SaveData& data) { apply_save_data(data); }

private:
    std::filesystem::path get_slot_path(SaveSlotId slot) const;
    std::string generate_slot_filename(SaveSlotId slot) const;
    SaveData gather_save_data(const std::string& name, SaveType type) const;
    void apply_save_data(const SaveData& data);
    void notify_save(SaveSlotId slot, SaveType type, SaveResult result, const std::string& error = "");
    void notify_load(SaveSlotId slot, LoadResult result, const std::string& error = "");

    std::filesystem::path m_save_dir;
    std::uint32_t m_max_slots{10};
    std::string m_game_version;
    std::string m_current_level;
    double m_play_time{0};

    std::vector<ISaveable*> m_saveables;
    std::vector<SaveSlot> m_slots;
    SaveSlotId m_quick_save_slot{1};

    SaveSerializer m_serializer;
    SaveCallback m_on_save;
    LoadCallback m_on_load;

    bool m_is_saving{false};
    bool m_is_loading{false};
    float m_progress{0};
};

// =============================================================================
// AutoSaveManager
// =============================================================================

/// @brief Manages automatic saves
class AutoSaveManager {
public:
    AutoSaveManager();
    explicit AutoSaveManager(SaveManager* save_manager);
    ~AutoSaveManager();

    // Configuration
    void set_save_manager(SaveManager* manager) { m_save_manager = manager; }
    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }
    void set_interval(float seconds) { m_interval = seconds; }
    float interval() const { return m_interval; }
    void set_max_auto_saves(std::uint32_t count) { m_max_auto_saves = count; }
    std::uint32_t max_auto_saves() const { return m_max_auto_saves; }

    // Control
    void enable();
    void disable();
    void pause();
    void resume();
    bool is_paused() const { return m_paused; }

    // Update
    void update(float delta_time);

    // Manual trigger
    void trigger_auto_save();

    // State
    float time_until_next() const;
    double last_auto_save_time() const { return m_last_save_time; }
    std::uint32_t auto_save_count() const { return m_auto_save_count; }

    // Conditions
    void set_save_condition(std::function<bool()> condition) { m_condition = std::move(condition); }
    void add_blocking_condition(const std::string& id, std::function<bool()> condition);
    void remove_blocking_condition(const std::string& id);
    void clear_blocking_conditions();

    // Callbacks
    void set_on_auto_save(std::function<void()> callback) { m_on_auto_save = std::move(callback); }

private:
    SaveSlotId get_next_auto_save_slot() const;
    bool can_auto_save() const;

    SaveManager* m_save_manager{nullptr};
    bool m_enabled{true};
    bool m_paused{false};
    float m_interval{300.0f}; // 5 minutes
    std::uint32_t m_max_auto_saves{3};

    float m_timer{0};
    double m_last_save_time{0};
    std::uint32_t m_auto_save_count{0};
    std::uint32_t m_current_slot_index{0};

    std::function<bool()> m_condition;
    std::unordered_map<std::string, std::function<bool()>> m_blocking_conditions;
    std::function<void()> m_on_auto_save;
};

// =============================================================================
// CheckpointManager
// =============================================================================

/// @brief Manages checkpoint saves
class CheckpointManager {
public:
    struct Checkpoint {
        CheckpointId id;
        std::string name;
        std::string description;
        Vec3 position;
        Vec3 rotation;
        std::string level_name;
        double timestamp{0};
        std::vector<std::uint8_t> state_data;
        std::unordered_map<std::string, std::string> metadata;
    };

    CheckpointManager();
    explicit CheckpointManager(SaveManager* save_manager);
    ~CheckpointManager();

    // Configuration
    void set_save_manager(SaveManager* manager) { m_save_manager = manager; }
    void set_max_checkpoints(std::uint32_t count) { m_max_checkpoints = count; }
    std::uint32_t max_checkpoints() const { return m_max_checkpoints; }
    void set_enabled(bool enabled) { m_enabled = enabled; }
    bool is_enabled() const { return m_enabled; }

    // Checkpoint operations
    CheckpointId create_checkpoint(const std::string& name = "");
    CheckpointId create_checkpoint(const std::string& name, const Vec3& position, const Vec3& rotation);
    bool delete_checkpoint(CheckpointId id);
    void clear_all_checkpoints();

    // Load checkpoint
    bool load_checkpoint(CheckpointId id);
    bool load_latest_checkpoint();

    // Query
    Checkpoint get_checkpoint(CheckpointId id) const;
    std::vector<Checkpoint> get_all_checkpoints() const;
    CheckpointId get_latest_checkpoint() const;
    bool has_checkpoint(CheckpointId id) const;
    std::size_t checkpoint_count() const { return m_checkpoints.size(); }

    // Named checkpoints
    CheckpointId find_checkpoint(const std::string& name) const;
    std::vector<Checkpoint> get_checkpoints_in_level(const std::string& level) const;

    // Callbacks
    void set_on_checkpoint_created(std::function<void(CheckpointId)> callback) {
        m_on_created = std::move(callback);
    }
    void set_on_checkpoint_loaded(std::function<void(CheckpointId)> callback) {
        m_on_loaded = std::move(callback);
    }

    // Serialization
    std::vector<std::uint8_t> serialize() const;
    void deserialize(const std::vector<std::uint8_t>& data);

    // Update
    void set_current_position(const Vec3& position) { m_current_position = position; }
    void set_current_rotation(const Vec3& rotation) { m_current_rotation = rotation; }
    void set_current_level(const std::string& level) { m_current_level = level; }
    void set_current_time(double time) { m_current_time = time; }

private:
    std::vector<std::uint8_t> capture_state() const;
    void restore_state(const std::vector<std::uint8_t>& data);

    SaveManager* m_save_manager{nullptr};
    std::uint32_t m_max_checkpoints{50};
    bool m_enabled{true};

    std::unordered_map<CheckpointId, Checkpoint> m_checkpoints;
    std::uint64_t m_next_id{1};

    Vec3 m_current_position;
    Vec3 m_current_rotation;
    std::string m_current_level;
    double m_current_time{0};

    std::function<void(CheckpointId)> m_on_created;
    std::function<void(CheckpointId)> m_on_loaded;
};

// =============================================================================
// SaveStateSnapshot
// =============================================================================

/// @brief In-memory save state for quick saves/reloads
class SaveStateSnapshot {
public:
    SaveStateSnapshot();
    ~SaveStateSnapshot();

    // Capture/Restore
    void capture(SaveManager* manager);
    void restore(SaveManager* manager);
    bool is_valid() const { return !m_data.variable_data.empty(); }

    // Metadata
    const SaveMetadata& metadata() const { return m_data.metadata; }
    double timestamp() const { return m_data.metadata.timestamp; }

    // Clear
    void clear();

private:
    SaveData m_data;
};

// =============================================================================
// SaveMigrator
// =============================================================================

/// @brief Handles save file version migrations
class SaveMigrator {
public:
    using MigrationFunc = std::function<bool(SaveData&, std::uint32_t from, std::uint32_t to)>;

    SaveMigrator();
    ~SaveMigrator();

    // Version management
    void set_current_version(std::uint32_t version) { m_current_version = version; }
    std::uint32_t current_version() const { return m_current_version; }

    // Migration registration
    void register_migration(std::uint32_t from_version, std::uint32_t to_version, MigrationFunc func);

    // Migration execution
    bool can_migrate(std::uint32_t from_version) const;
    bool migrate(SaveData& data) const;
    std::vector<std::uint32_t> get_migration_path(std::uint32_t from_version) const;

private:
    struct Migration {
        std::uint32_t from_version;
        std::uint32_t to_version;
        MigrationFunc func;
    };

    std::uint32_t m_current_version{1};
    std::vector<Migration> m_migrations;
};

} // namespace void_gamestate
