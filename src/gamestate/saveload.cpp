/// @file saveload.cpp
/// @brief Implementation of save/load system for void_gamestate module

#include "void_engine/gamestate/saveload.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>

namespace void_gamestate {

// =============================================================================
// SaveSerializer
// =============================================================================

SaveSerializer::SaveSerializer() = default;
SaveSerializer::~SaveSerializer() = default;

std::vector<std::uint8_t> SaveSerializer::serialize(const SaveData& data) const {
    std::vector<std::uint8_t> result;

    // Header
    const char* magic = "VOID";
    result.insert(result.end(), magic, magic + 4);

    // Version
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t),
                &data.metadata.save_version, sizeof(std::uint32_t));

    // Serialize metadata
    auto meta_bytes = serialize_metadata(data.metadata);
    std::uint32_t meta_size = static_cast<std::uint32_t>(meta_bytes.size());
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t),
                &meta_size, sizeof(std::uint32_t));
    result.insert(result.end(), meta_bytes.begin(), meta_bytes.end());

    // Variable data
    std::uint32_t var_size = static_cast<std::uint32_t>(data.variable_data.size());
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t),
                &var_size, sizeof(std::uint32_t));
    result.insert(result.end(), data.variable_data.begin(), data.variable_data.end());

    // Entity data
    std::uint32_t entity_size = static_cast<std::uint32_t>(data.entity_data.size());
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t),
                &entity_size, sizeof(std::uint32_t));
    result.insert(result.end(), data.entity_data.begin(), data.entity_data.end());

    // World data
    std::uint32_t world_size = static_cast<std::uint32_t>(data.world_data.size());
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t),
                &world_size, sizeof(std::uint32_t));
    result.insert(result.end(), data.world_data.begin(), data.world_data.end());

    // Custom data
    std::uint32_t custom_size = static_cast<std::uint32_t>(data.custom_data.size());
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t),
                &custom_size, sizeof(std::uint32_t));
    result.insert(result.end(), data.custom_data.begin(), data.custom_data.end());

    // Checksum
    std::uint32_t checksum = calculate_checksum(result);
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t),
                &checksum, sizeof(std::uint32_t));

    // Compress if enabled
    if (m_compress) {
        result = compress(result);
    }

    // Encrypt if enabled
    if (m_encrypt) {
        result = encrypt(result);
    }

    return result;
}

SaveData SaveSerializer::deserialize(const std::vector<std::uint8_t>& bytes) const {
    SaveData data;
    auto working = bytes;

    // Decrypt if needed
    if (m_encrypt) {
        working = decrypt(working);
    }

    // Decompress if needed
    if (m_compress) {
        working = decompress(working);
    }

    if (working.size() < 8) {
        return data;
    }

    std::size_t pos = 0;

    // Check magic
    if (std::memcmp(working.data(), "VOID", 4) != 0) {
        return data;
    }
    pos += 4;

    // Version
    std::memcpy(&data.metadata.save_version, working.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);

    // Metadata
    std::uint32_t meta_size;
    std::memcpy(&meta_size, working.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);
    if (pos + meta_size > working.size()) return data;
    std::vector<std::uint8_t> meta_bytes(working.begin() + pos, working.begin() + pos + meta_size);
    data.metadata = deserialize_metadata(meta_bytes);
    pos += meta_size;

    // Variable data
    std::uint32_t var_size;
    std::memcpy(&var_size, working.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);
    if (pos + var_size > working.size()) return data;
    data.variable_data.assign(working.begin() + pos, working.begin() + pos + var_size);
    pos += var_size;

    // Entity data
    std::uint32_t entity_size;
    std::memcpy(&entity_size, working.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);
    if (pos + entity_size > working.size()) return data;
    data.entity_data.assign(working.begin() + pos, working.begin() + pos + entity_size);
    pos += entity_size;

    // World data
    std::uint32_t world_size;
    std::memcpy(&world_size, working.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);
    if (pos + world_size > working.size()) return data;
    data.world_data.assign(working.begin() + pos, working.begin() + pos + world_size);
    pos += world_size;

    // Custom data
    std::uint32_t custom_size;
    std::memcpy(&custom_size, working.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);
    if (pos + custom_size > working.size()) return data;
    data.custom_data.assign(working.begin() + pos, working.begin() + pos + custom_size);
    pos += custom_size;

    // Checksum
    std::memcpy(&data.checksum, working.data() + pos, sizeof(std::uint32_t));

    return data;
}

std::vector<std::uint8_t> SaveSerializer::serialize_metadata(const SaveMetadata& metadata) const {
    std::vector<std::uint8_t> result;

    // Slot ID
    result.resize(result.size() + sizeof(std::uint64_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint64_t),
                &metadata.slot_id.value, sizeof(std::uint64_t));

    // Name length + name
    std::uint32_t name_len = static_cast<std::uint32_t>(metadata.name.size());
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t), &name_len, sizeof(std::uint32_t));
    result.insert(result.end(), metadata.name.begin(), metadata.name.end());

    // Type
    result.push_back(static_cast<std::uint8_t>(metadata.type));

    // Timestamp
    result.resize(result.size() + sizeof(double));
    std::memcpy(result.data() + result.size() - sizeof(double), &metadata.timestamp, sizeof(double));

    // Play time
    result.resize(result.size() + sizeof(double));
    std::memcpy(result.data() + result.size() - sizeof(double), &metadata.play_time, sizeof(double));

    // Game version
    std::uint32_t ver_len = static_cast<std::uint32_t>(metadata.game_version.size());
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t), &ver_len, sizeof(std::uint32_t));
    result.insert(result.end(), metadata.game_version.begin(), metadata.game_version.end());

    // Level name
    std::uint32_t level_len = static_cast<std::uint32_t>(metadata.level_name.size());
    result.resize(result.size() + sizeof(std::uint32_t));
    std::memcpy(result.data() + result.size() - sizeof(std::uint32_t), &level_len, sizeof(std::uint32_t));
    result.insert(result.end(), metadata.level_name.begin(), metadata.level_name.end());

    return result;
}

SaveMetadata SaveSerializer::deserialize_metadata(const std::vector<std::uint8_t>& bytes) const {
    SaveMetadata metadata;
    if (bytes.empty()) return metadata;

    std::size_t pos = 0;

    // Slot ID
    if (pos + sizeof(std::uint64_t) > bytes.size()) return metadata;
    std::memcpy(&metadata.slot_id.value, bytes.data() + pos, sizeof(std::uint64_t));
    pos += sizeof(std::uint64_t);

    // Name
    if (pos + sizeof(std::uint32_t) > bytes.size()) return metadata;
    std::uint32_t name_len;
    std::memcpy(&name_len, bytes.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);
    if (pos + name_len > bytes.size()) return metadata;
    metadata.name.assign(reinterpret_cast<const char*>(bytes.data() + pos), name_len);
    pos += name_len;

    // Type
    if (pos >= bytes.size()) return metadata;
    metadata.type = static_cast<SaveType>(bytes[pos++]);

    // Timestamp
    if (pos + sizeof(double) > bytes.size()) return metadata;
    std::memcpy(&metadata.timestamp, bytes.data() + pos, sizeof(double));
    pos += sizeof(double);

    // Play time
    if (pos + sizeof(double) > bytes.size()) return metadata;
    std::memcpy(&metadata.play_time, bytes.data() + pos, sizeof(double));
    pos += sizeof(double);

    // Game version
    if (pos + sizeof(std::uint32_t) > bytes.size()) return metadata;
    std::uint32_t ver_len;
    std::memcpy(&ver_len, bytes.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);
    if (pos + ver_len > bytes.size()) return metadata;
    metadata.game_version.assign(reinterpret_cast<const char*>(bytes.data() + pos), ver_len);
    pos += ver_len;

    // Level name
    if (pos + sizeof(std::uint32_t) > bytes.size()) return metadata;
    std::uint32_t level_len;
    std::memcpy(&level_len, bytes.data() + pos, sizeof(std::uint32_t));
    pos += sizeof(std::uint32_t);
    if (pos + level_len > bytes.size()) return metadata;
    metadata.level_name.assign(reinterpret_cast<const char*>(bytes.data() + pos), level_len);

    return metadata;
}

std::uint32_t SaveSerializer::calculate_checksum(const std::vector<std::uint8_t>& data) const {
    // Simple CRC-like checksum
    std::uint32_t checksum = 0;
    for (auto byte : data) {
        checksum = (checksum << 1) | (checksum >> 31);
        checksum ^= byte;
    }
    return checksum;
}

bool SaveSerializer::verify_checksum(const std::vector<std::uint8_t>& data, std::uint32_t checksum) const {
    return calculate_checksum(data) == checksum;
}

std::vector<std::uint8_t> SaveSerializer::compress(const std::vector<std::uint8_t>& data) const {
    // Simple RLE compression for demonstration
    // Production would use zlib, lz4, etc.
    std::vector<std::uint8_t> result;
    result.push_back(0x01); // Compression flag

    std::size_t i = 0;
    while (i < data.size()) {
        std::uint8_t byte = data[i];
        std::uint8_t count = 1;

        while (i + count < data.size() && data[i + count] == byte && count < 255) {
            ++count;
        }

        if (count > 3 || byte == 0xFF) {
            result.push_back(0xFF);
            result.push_back(count);
            result.push_back(byte);
        } else {
            for (std::uint8_t j = 0; j < count; ++j) {
                result.push_back(byte);
            }
        }

        i += count;
    }

    // Only use compressed if smaller
    if (result.size() >= data.size()) {
        result.clear();
        result.push_back(0x00); // No compression
        result.insert(result.end(), data.begin(), data.end());
    }

    return result;
}

std::vector<std::uint8_t> SaveSerializer::decompress(const std::vector<std::uint8_t>& data) const {
    if (data.empty()) return {};

    std::vector<std::uint8_t> result;

    if (data[0] == 0x00) {
        // Not compressed
        result.assign(data.begin() + 1, data.end());
    } else if (data[0] == 0x01) {
        // RLE compressed
        std::size_t i = 1;
        while (i < data.size()) {
            if (data[i] == 0xFF && i + 2 < data.size()) {
                std::uint8_t count = data[i + 1];
                std::uint8_t byte = data[i + 2];
                for (std::uint8_t j = 0; j < count; ++j) {
                    result.push_back(byte);
                }
                i += 3;
            } else {
                result.push_back(data[i]);
                ++i;
            }
        }
    } else {
        // Unknown compression
        result.assign(data.begin() + 1, data.end());
    }

    return result;
}

std::vector<std::uint8_t> SaveSerializer::encrypt(const std::vector<std::uint8_t>& data) const {
    // Simple XOR encryption for demonstration
    // Production would use AES, etc.
    std::vector<std::uint8_t> result = data;

    if (m_encryption_key.empty()) {
        return result;
    }

    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] ^= m_encryption_key[i % m_encryption_key.size()];
    }

    return result;
}

std::vector<std::uint8_t> SaveSerializer::decrypt(const std::vector<std::uint8_t>& data) const {
    // XOR is symmetric
    return encrypt(data);
}

// =============================================================================
// SaveManager
// =============================================================================

SaveManager::SaveManager() {
    m_save_dir = "saves";
}

SaveManager::SaveManager(const GameStateConfig& config)
    : m_save_dir(config.save_directory)
    , m_max_slots(config.max_save_slots) {
    m_serializer.set_compression(config.compress_saves);
    m_serializer.set_encryption(config.encrypt_saves);
}

SaveManager::~SaveManager() = default;

void SaveManager::set_save_directory(const std::filesystem::path& path) {
    m_save_dir = path;
    if (!std::filesystem::exists(m_save_dir)) {
        std::filesystem::create_directories(m_save_dir);
    }
    refresh_slots();
}

std::vector<SaveSlot> SaveManager::get_all_slots() const {
    return m_slots;
}

SaveSlot SaveManager::get_slot(SaveSlotId slot) const {
    for (const auto& s : m_slots) {
        if (s.id == slot) {
            return s;
        }
    }
    return SaveSlot{slot};
}

SaveSlotId SaveManager::get_empty_slot() const {
    for (std::uint32_t i = 1; i <= m_max_slots; ++i) {
        SaveSlotId id{i};
        bool found = false;
        for (const auto& s : m_slots) {
            if (s.id == id && !s.is_empty) {
                found = true;
                break;
            }
        }
        if (!found) {
            return id;
        }
    }
    return SaveSlotId{};
}

SaveSlotId SaveManager::get_latest_slot() const {
    SaveSlotId latest;
    double latest_time = 0;

    for (const auto& s : m_slots) {
        if (!s.is_empty && s.metadata.timestamp > latest_time) {
            latest = s.id;
            latest_time = s.metadata.timestamp;
        }
    }

    return latest;
}

bool SaveManager::is_slot_empty(SaveSlotId slot) const {
    return get_slot(slot).is_empty;
}

bool SaveManager::delete_slot(SaveSlotId slot) {
    auto path = get_slot_path(slot);
    if (std::filesystem::exists(path)) {
        std::filesystem::remove(path);
        refresh_slots();
        return true;
    }
    return false;
}

void SaveManager::register_saveable(ISaveable* saveable) {
    if (saveable) {
        m_saveables.push_back(saveable);
    }
}

void SaveManager::unregister_saveable(ISaveable* saveable) {
    m_saveables.erase(
        std::remove(m_saveables.begin(), m_saveables.end(), saveable),
        m_saveables.end());
}

void SaveManager::unregister_saveable(const std::string& id) {
    m_saveables.erase(
        std::remove_if(m_saveables.begin(), m_saveables.end(),
                       [&id](ISaveable* s) { return s && s->saveable_id() == id; }),
        m_saveables.end());
}

SaveResult SaveManager::save(SaveSlotId slot, const std::string& name, SaveType type) {
    if (!slot) {
        slot = get_empty_slot();
        if (!slot) {
            notify_save(slot, type, SaveResult::NoSpace);
            return SaveResult::NoSpace;
        }
    }

    m_is_saving = true;
    m_progress = 0;

    // Notify saveables
    for (auto* saveable : m_saveables) {
        if (saveable) {
            saveable->on_before_save();
        }
    }

    // Gather save data
    SaveData data = gather_save_data(name, type);
    data.metadata.slot_id = slot;

    // Serialize
    m_progress = 0.3f;
    auto bytes = m_serializer.serialize(data);

    // Write to file
    m_progress = 0.7f;
    auto path = get_slot_path(slot);

    try {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::binary);
        if (!file) {
            m_is_saving = false;
            notify_save(slot, type, SaveResult::Failed, "Failed to open file");
            return SaveResult::Failed;
        }
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        file.close();
    } catch (const std::exception& e) {
        m_is_saving = false;
        notify_save(slot, type, SaveResult::Failed, e.what());
        return SaveResult::Failed;
    }

    m_progress = 1.0f;
    m_is_saving = false;

    // Notify saveables
    for (auto* saveable : m_saveables) {
        if (saveable) {
            saveable->on_after_save(SaveResult::Success);
        }
    }

    refresh_slots();
    notify_save(slot, type, SaveResult::Success);
    return SaveResult::Success;
}

SaveResult SaveManager::save_async(SaveSlotId slot, const std::string& name, SaveType type) {
    // In production, this would use a thread pool
    return save(slot, name, type);
}

SaveResult SaveManager::quick_save() {
    return save(m_quick_save_slot, "Quick Save", SaveType::Quick);
}

LoadResult SaveManager::load(SaveSlotId slot) {
    if (!slot) {
        slot = get_latest_slot();
        if (!slot) {
            notify_load(slot, LoadResult::NotFound);
            return LoadResult::NotFound;
        }
    }

    auto path = get_slot_path(slot);
    if (!std::filesystem::exists(path)) {
        notify_load(slot, LoadResult::NotFound);
        return LoadResult::NotFound;
    }

    m_is_loading = true;
    m_progress = 0;

    // Notify saveables
    for (auto* saveable : m_saveables) {
        if (saveable) {
            saveable->on_before_load();
        }
    }

    // Read file
    std::vector<std::uint8_t> bytes;
    try {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            m_is_loading = false;
            notify_load(slot, LoadResult::Failed, "Failed to open file");
            return LoadResult::Failed;
        }
        auto size = file.tellg();
        file.seekg(0);
        bytes.resize(size);
        file.read(reinterpret_cast<char*>(bytes.data()), size);
    } catch (const std::exception& e) {
        m_is_loading = false;
        notify_load(slot, LoadResult::Failed, e.what());
        return LoadResult::Failed;
    }

    m_progress = 0.3f;

    // Deserialize
    SaveData data = m_serializer.deserialize(bytes);
    if (data.metadata.slot_id.value == 0) {
        m_is_loading = false;
        notify_load(slot, LoadResult::Corrupted);
        return LoadResult::Corrupted;
    }

    m_progress = 0.6f;

    // Version check
    if (!is_compatible(data.metadata)) {
        m_is_loading = false;
        notify_load(slot, LoadResult::VersionMismatch);
        return LoadResult::VersionMismatch;
    }

    // Apply data
    m_progress = 0.8f;
    apply_save_data(data);

    m_progress = 1.0f;
    m_is_loading = false;

    // Notify saveables
    for (auto* saveable : m_saveables) {
        if (saveable) {
            saveable->on_after_load(LoadResult::Success);
        }
    }

    notify_load(slot, LoadResult::Success);
    return LoadResult::Success;
}

LoadResult SaveManager::load_async(SaveSlotId slot) {
    // In production, this would use a thread pool
    return load(slot);
}

LoadResult SaveManager::quick_load() {
    return load(m_quick_save_slot);
}

SaveMetadata SaveManager::get_metadata(SaveSlotId slot) const {
    return get_slot(slot).metadata;
}

std::vector<SaveMetadata> SaveManager::get_all_metadata() const {
    std::vector<SaveMetadata> result;
    for (const auto& s : m_slots) {
        if (!s.is_empty) {
            result.push_back(s.metadata);
        }
    }
    return result;
}

SaveResult SaveManager::export_save(SaveSlotId slot, const std::filesystem::path& path) const {
    auto src_path = get_slot_path(slot);
    if (!std::filesystem::exists(src_path)) {
        return SaveResult::Failed;
    }

    try {
        std::filesystem::copy_file(src_path, path, std::filesystem::copy_options::overwrite_existing);
        return SaveResult::Success;
    } catch (...) {
        return SaveResult::Failed;
    }
}

LoadResult SaveManager::import_save(const std::filesystem::path& path, SaveSlotId slot) {
    if (!std::filesystem::exists(path)) {
        return LoadResult::NotFound;
    }

    if (!validate_save_file(path)) {
        return LoadResult::Corrupted;
    }

    try {
        auto dest_path = get_slot_path(slot);
        std::filesystem::create_directories(dest_path.parent_path());
        std::filesystem::copy_file(path, dest_path, std::filesystem::copy_options::overwrite_existing);
        refresh_slots();
        return LoadResult::Success;
    } catch (...) {
        return LoadResult::Failed;
    }
}

bool SaveManager::validate_save(SaveSlotId slot) const {
    return validate_save_file(get_slot_path(slot));
}

bool SaveManager::validate_save_file(const std::filesystem::path& path) const {
    if (!std::filesystem::exists(path)) {
        return false;
    }

    try {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        char magic[4];
        file.read(magic, 4);
        return std::memcmp(magic, "VOID", 4) == 0;
    } catch (...) {
        return false;
    }
}

bool SaveManager::is_compatible(const SaveMetadata& metadata) const {
    // Check version compatibility
    // In production, this would have version migration logic
    return true;
}

void SaveManager::refresh_slots() {
    m_slots.clear();

    if (!std::filesystem::exists(m_save_dir)) {
        return;
    }

    for (std::uint32_t i = 1; i <= m_max_slots; ++i) {
        SaveSlotId id{i};
        auto path = get_slot_path(id);

        SaveSlot slot;
        slot.id = id;
        slot.file_path = path.string();

        if (std::filesystem::exists(path)) {
            slot.is_empty = false;
            slot.file_size = std::filesystem::file_size(path);

            // Read metadata
            try {
                std::ifstream file(path, std::ios::binary | std::ios::ate);
                if (file) {
                    auto size = file.tellg();
                    file.seekg(0);
                    std::vector<std::uint8_t> bytes(size);
                    file.read(reinterpret_cast<char*>(bytes.data()), size);

                    auto data = m_serializer.deserialize(bytes);
                    slot.metadata = data.metadata;
                    slot.is_corrupted = (data.metadata.slot_id.value == 0);
                }
            } catch (...) {
                slot.is_corrupted = true;
            }
        } else {
            slot.is_empty = true;
        }

        m_slots.push_back(slot);
    }
}

std::uint64_t SaveManager::get_total_save_size() const {
    std::uint64_t total = 0;
    for (const auto& s : m_slots) {
        total += s.file_size;
    }
    return total;
}

std::filesystem::path SaveManager::get_slot_path(SaveSlotId slot) const {
    return m_save_dir / generate_slot_filename(slot);
}

std::string SaveManager::generate_slot_filename(SaveSlotId slot) const {
    return "save_" + std::to_string(slot.value) + ".sav";
}

SaveData SaveManager::gather_save_data(const std::string& name, SaveType type) const {
    SaveData data;

    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration<double>(now.time_since_epoch()).count();

    data.metadata.name = name;
    data.metadata.type = type;
    data.metadata.timestamp = timestamp;
    data.metadata.play_time = m_play_time;
    data.metadata.game_version = m_game_version;
    data.metadata.level_name = m_current_level;

    // Gather data from saveables
    for (auto* saveable : m_saveables) {
        if (saveable) {
            auto bytes = saveable->serialize();
            // Would store by ID in custom_data
        }
    }

    return data;
}

void SaveManager::apply_save_data(const SaveData& data) {
    // Apply data to saveables
    for (auto* saveable : m_saveables) {
        if (saveable) {
            // Would retrieve by ID from custom_data
            // saveable->deserialize(bytes, version);
        }
    }
}

void SaveManager::notify_save(SaveSlotId slot, SaveType type, SaveResult result, const std::string& error) {
    if (m_on_save) {
        SaveEvent event{
            .slot = slot,
            .type = type,
            .result = result,
            .error_message = error,
            .timestamp = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()).count()
        };
        m_on_save(event);
    }
}

void SaveManager::notify_load(SaveSlotId slot, LoadResult result, const std::string& error) {
    if (m_on_load) {
        LoadEvent event{
            .slot = slot,
            .result = result,
            .error_message = error,
            .timestamp = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch()).count()
        };
        m_on_load(event);
    }
}

// =============================================================================
// AutoSaveManager
// =============================================================================

AutoSaveManager::AutoSaveManager() = default;

AutoSaveManager::AutoSaveManager(SaveManager* save_manager)
    : m_save_manager(save_manager) {}

AutoSaveManager::~AutoSaveManager() = default;

void AutoSaveManager::enable() {
    m_enabled = true;
    m_paused = false;
}

void AutoSaveManager::disable() {
    m_enabled = false;
}

void AutoSaveManager::pause() {
    m_paused = true;
}

void AutoSaveManager::resume() {
    m_paused = false;
}

void AutoSaveManager::update(float delta_time) {
    if (!m_enabled || m_paused || !m_save_manager) {
        return;
    }

    m_timer += delta_time;

    if (m_timer >= m_interval && can_auto_save()) {
        trigger_auto_save();
    }
}

void AutoSaveManager::trigger_auto_save() {
    if (!m_save_manager) return;

    auto slot = get_next_auto_save_slot();
    auto result = m_save_manager->save(slot, "Auto Save", SaveType::Auto);

    if (result == SaveResult::Success) {
        m_timer = 0;
        m_last_save_time = std::chrono::duration<double>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ++m_auto_save_count;
        m_current_slot_index = (m_current_slot_index + 1) % m_max_auto_saves;

        if (m_on_auto_save) {
            m_on_auto_save();
        }
    }
}

float AutoSaveManager::time_until_next() const {
    return std::max(0.0f, m_interval - m_timer);
}

void AutoSaveManager::add_blocking_condition(const std::string& id, std::function<bool()> condition) {
    m_blocking_conditions[id] = std::move(condition);
}

void AutoSaveManager::remove_blocking_condition(const std::string& id) {
    m_blocking_conditions.erase(id);
}

void AutoSaveManager::clear_blocking_conditions() {
    m_blocking_conditions.clear();
}

SaveSlotId AutoSaveManager::get_next_auto_save_slot() const {
    // Use slots after the manual save slots
    // Assuming manual slots are 1-10, auto saves use 11-13
    return SaveSlotId{m_save_manager->max_slots() + 1 + m_current_slot_index};
}

bool AutoSaveManager::can_auto_save() const {
    if (m_condition && !m_condition()) {
        return false;
    }

    for (const auto& [_, cond] : m_blocking_conditions) {
        if (cond && cond()) {
            return false;
        }
    }

    return true;
}

// =============================================================================
// CheckpointManager
// =============================================================================

CheckpointManager::CheckpointManager() = default;

CheckpointManager::CheckpointManager(SaveManager* save_manager)
    : m_save_manager(save_manager) {}

CheckpointManager::~CheckpointManager() = default;

CheckpointId CheckpointManager::create_checkpoint(const std::string& name) {
    return create_checkpoint(name, m_current_position, m_current_rotation);
}

CheckpointId CheckpointManager::create_checkpoint(const std::string& name, const Vec3& position, const Vec3& rotation) {
    if (!m_enabled) {
        return CheckpointId{};
    }

    // Remove oldest if at limit
    while (m_checkpoints.size() >= m_max_checkpoints) {
        // Find oldest
        CheckpointId oldest;
        double oldest_time = std::numeric_limits<double>::max();
        for (const auto& [id, cp] : m_checkpoints) {
            if (cp.timestamp < oldest_time) {
                oldest = id;
                oldest_time = cp.timestamp;
            }
        }
        if (oldest) {
            m_checkpoints.erase(oldest);
        }
    }

    CheckpointId id{m_next_id++};

    Checkpoint cp;
    cp.id = id;
    cp.name = name.empty() ? "Checkpoint " + std::to_string(id.value) : name;
    cp.position = position;
    cp.rotation = rotation;
    cp.level_name = m_current_level;
    cp.timestamp = m_current_time;
    cp.state_data = capture_state();

    m_checkpoints[id] = std::move(cp);

    if (m_on_created) {
        m_on_created(id);
    }

    return id;
}

bool CheckpointManager::delete_checkpoint(CheckpointId id) {
    return m_checkpoints.erase(id) > 0;
}

void CheckpointManager::clear_all_checkpoints() {
    m_checkpoints.clear();
}

bool CheckpointManager::load_checkpoint(CheckpointId id) {
    auto it = m_checkpoints.find(id);
    if (it == m_checkpoints.end()) {
        return false;
    }

    restore_state(it->second.state_data);

    if (m_on_loaded) {
        m_on_loaded(id);
    }

    return true;
}

bool CheckpointManager::load_latest_checkpoint() {
    auto latest = get_latest_checkpoint();
    if (!latest) {
        return false;
    }
    return load_checkpoint(latest);
}

CheckpointManager::Checkpoint CheckpointManager::get_checkpoint(CheckpointId id) const {
    auto it = m_checkpoints.find(id);
    return it != m_checkpoints.end() ? it->second : Checkpoint{};
}

std::vector<CheckpointManager::Checkpoint> CheckpointManager::get_all_checkpoints() const {
    std::vector<Checkpoint> result;
    result.reserve(m_checkpoints.size());
    for (const auto& [_, cp] : m_checkpoints) {
        result.push_back(cp);
    }
    // Sort by timestamp
    std::sort(result.begin(), result.end(),
              [](const Checkpoint& a, const Checkpoint& b) { return a.timestamp < b.timestamp; });
    return result;
}

CheckpointId CheckpointManager::get_latest_checkpoint() const {
    CheckpointId latest;
    double latest_time = 0;

    for (const auto& [id, cp] : m_checkpoints) {
        if (cp.timestamp > latest_time) {
            latest = id;
            latest_time = cp.timestamp;
        }
    }

    return latest;
}

bool CheckpointManager::has_checkpoint(CheckpointId id) const {
    return m_checkpoints.contains(id);
}

CheckpointId CheckpointManager::find_checkpoint(const std::string& name) const {
    for (const auto& [id, cp] : m_checkpoints) {
        if (cp.name == name) {
            return id;
        }
    }
    return CheckpointId{};
}

std::vector<CheckpointManager::Checkpoint> CheckpointManager::get_checkpoints_in_level(const std::string& level) const {
    std::vector<Checkpoint> result;
    for (const auto& [_, cp] : m_checkpoints) {
        if (cp.level_name == level) {
            result.push_back(cp);
        }
    }
    return result;
}

std::vector<std::uint8_t> CheckpointManager::serialize() const {
    std::vector<std::uint8_t> result;
    // Serialize checkpoints
    return result;
}

void CheckpointManager::deserialize(const std::vector<std::uint8_t>& data) {
    // Deserialize checkpoints
}

std::vector<std::uint8_t> CheckpointManager::capture_state() const {
    std::vector<std::uint8_t> state;
    // Capture current state from save manager
    return state;
}

void CheckpointManager::restore_state(const std::vector<std::uint8_t>& data) {
    // Restore state through save manager
}

// =============================================================================
// SaveStateSnapshot
// =============================================================================

SaveStateSnapshot::SaveStateSnapshot() = default;
SaveStateSnapshot::~SaveStateSnapshot() = default;

void SaveStateSnapshot::capture(SaveManager* manager) {
    if (!manager) return;
    // Capture current state
}

void SaveStateSnapshot::restore(SaveManager* manager) {
    if (!manager || !is_valid()) return;
    // Restore captured state
}

void SaveStateSnapshot::clear() {
    m_data = SaveData{};
}

// =============================================================================
// SaveMigrator
// =============================================================================

SaveMigrator::SaveMigrator() = default;
SaveMigrator::~SaveMigrator() = default;

void SaveMigrator::register_migration(std::uint32_t from_version, std::uint32_t to_version, MigrationFunc func) {
    m_migrations.push_back({from_version, to_version, std::move(func)});
}

bool SaveMigrator::can_migrate(std::uint32_t from_version) const {
    auto path = get_migration_path(from_version);
    return !path.empty();
}

bool SaveMigrator::migrate(SaveData& data) const {
    auto path = get_migration_path(data.metadata.save_version);
    if (path.empty()) {
        return false;
    }

    for (std::size_t i = 0; i < path.size() - 1; ++i) {
        std::uint32_t from = path[i];
        std::uint32_t to = path[i + 1];

        // Find migration
        for (const auto& mig : m_migrations) {
            if (mig.from_version == from && mig.to_version == to) {
                if (!mig.func(data, from, to)) {
                    return false;
                }
                data.metadata.save_version = to;
                break;
            }
        }
    }

    return data.metadata.save_version == m_current_version;
}

std::vector<std::uint32_t> SaveMigrator::get_migration_path(std::uint32_t from_version) const {
    if (from_version == m_current_version) {
        return {from_version};
    }

    // BFS to find path
    std::vector<std::uint32_t> path;
    std::unordered_map<std::uint32_t, std::uint32_t> parent;
    std::vector<std::uint32_t> queue = {from_version};
    parent[from_version] = from_version;

    while (!queue.empty()) {
        std::uint32_t current = queue.front();
        queue.erase(queue.begin());

        if (current == m_current_version) {
            // Reconstruct path
            std::uint32_t node = current;
            while (node != from_version) {
                path.push_back(node);
                node = parent[node];
            }
            path.push_back(from_version);
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (const auto& mig : m_migrations) {
            if (mig.from_version == current && !parent.contains(mig.to_version)) {
                parent[mig.to_version] = current;
                queue.push_back(mig.to_version);
            }
        }
    }

    return {};
}

} // namespace void_gamestate
