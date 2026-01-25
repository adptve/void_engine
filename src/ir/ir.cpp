/// @file ir.cpp
/// @brief Main implementation file for void_ir module
///
/// This file provides:
/// - Module version information
/// - Module initialization
/// - Hot-reload factory functions
/// - IR system coordination

#include <void_engine/ir/ir.hpp>
#include <void_engine/core/hot_reload.hpp>

namespace void_ir {

namespace {
    constexpr std::uint32_t IR_MAGIC = 0x564F4944;  // "VOID"
    constexpr std::uint32_t IR_VERSION_MAJOR = Version::MAJOR;
    constexpr std::uint32_t IR_VERSION_MINOR = Version::MINOR;
    constexpr std::uint32_t IR_VERSION_PATCH = Version::PATCH;
}

const char* version() noexcept {
    return "1.0.0";
}

std::uint32_t version_major() noexcept {
    return IR_VERSION_MAJOR;
}

std::uint32_t version_minor() noexcept {
    return IR_VERSION_MINOR;
}

std::uint32_t version_patch() noexcept {
    return IR_VERSION_PATCH;
}

void init() {
}

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_patch_bus();
std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_async_patch_bus();
std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_batch_optimizer();
std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_batch_optimizer(
    BatchOptimizer::Options options);
std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_deduplicator();
std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_namespace_registry();
std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_transaction_queue();
std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_conflict_detector();
std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_schema_registry();

std::unique_ptr<void_core::HotReloadable> wrap_patch_bus(std::shared_ptr<PatchBus> bus);
std::unique_ptr<void_core::HotReloadable> wrap_async_patch_bus(std::shared_ptr<AsyncPatchBus> bus);
std::unique_ptr<void_core::HotReloadable> wrap_batch_optimizer(std::shared_ptr<BatchOptimizer> optimizer);
std::unique_ptr<void_core::HotReloadable> wrap_deduplicator(std::shared_ptr<PatchDeduplicator> dedup);
std::unique_ptr<void_core::HotReloadable> wrap_namespace_registry(std::shared_ptr<NamespaceRegistry> registry);
std::unique_ptr<void_core::HotReloadable> wrap_transaction_queue(std::shared_ptr<TransactionQueue> queue);
std::unique_ptr<void_core::HotReloadable> wrap_conflict_detector(std::shared_ptr<ConflictDetector> detector);
std::unique_ptr<void_core::HotReloadable> wrap_schema_registry(std::shared_ptr<SchemaRegistry> registry);

void serialize_patch(BinaryWriter& writer, const Patch& patch);
Patch deserialize_patch(BinaryReader& reader);

std::vector<std::uint8_t> serialize_patch_binary(const Patch& patch);
std::optional<Patch> deserialize_patch_binary(const std::vector<std::uint8_t>& data);
std::vector<std::uint8_t> serialize_patch_batch_binary(const PatchBatch& batch);
std::optional<PatchBatch> deserialize_patch_batch_binary(const std::vector<std::uint8_t>& data);

std::vector<std::uint8_t> serialize_value_binary(const Value& value);
std::optional<Value> deserialize_value_binary(const std::vector<std::uint8_t>& data);

std::vector<std::uint8_t> serialize_transaction_binary(const Transaction& tx);
std::optional<Transaction> deserialize_transaction_binary(const std::vector<std::uint8_t>& data);

std::vector<std::uint8_t> serialize_namespace_registry(const NamespaceRegistry& registry);
std::optional<NamespaceRegistry> deserialize_namespace_registry(const std::vector<std::uint8_t>& data);

std::vector<std::uint8_t> serialize_schema_registry(const SchemaRegistry& registry);
std::optional<SchemaRegistry> deserialize_schema_registry(const std::vector<std::uint8_t>& data);

class IRSystem {
public:
    IRSystem()
        : m_namespaces(std::make_shared<NamespaceRegistry>())
        , m_schemas(std::make_shared<SchemaRegistry>())
        , m_patch_bus(std::make_shared<PatchBus>())
        , m_async_bus(std::make_shared<AsyncPatchBus>())
        , m_optimizer(std::make_shared<BatchOptimizer>())
        , m_deduplicator(std::make_shared<PatchDeduplicator>())
        , m_transaction_queue(std::make_shared<TransactionQueue>())
        , m_conflict_detector(std::make_shared<ConflictDetector>())
        , m_snapshot_manager(std::make_shared<SnapshotManager>(100))
        , m_next_transaction_id(1) {}

    NamespaceRegistry& namespaces() { return *m_namespaces; }
    const NamespaceRegistry& namespaces() const { return *m_namespaces; }

    SchemaRegistry& schemas() { return *m_schemas; }
    const SchemaRegistry& schemas() const { return *m_schemas; }

    PatchBus& patch_bus() { return *m_patch_bus; }
    const PatchBus& patch_bus() const { return *m_patch_bus; }

    AsyncPatchBus& async_bus() { return *m_async_bus; }
    const AsyncPatchBus& async_bus() const { return *m_async_bus; }

    BatchOptimizer& optimizer() { return *m_optimizer; }
    const BatchOptimizer& optimizer() const { return *m_optimizer; }

    PatchDeduplicator& deduplicator() { return *m_deduplicator; }
    const PatchDeduplicator& deduplicator() const { return *m_deduplicator; }

    TransactionQueue& transaction_queue() { return *m_transaction_queue; }
    const TransactionQueue& transaction_queue() const { return *m_transaction_queue; }

    ConflictDetector& conflict_detector() { return *m_conflict_detector; }
    const ConflictDetector& conflict_detector() const { return *m_conflict_detector; }

    SnapshotManager& snapshots() { return *m_snapshot_manager; }
    const SnapshotManager& snapshots() const { return *m_snapshot_manager; }

    NamespaceId create_namespace(std::string name) {
        return m_namespaces->create(std::move(name));
    }

    NamespaceId create_namespace(std::string name, NamespacePermissions perms, ResourceLimits limits) {
        return m_namespaces->create(std::move(name), std::move(perms), limits);
    }

    TransactionId allocate_transaction_id() {
        return TransactionId{m_next_transaction_id++};
    }

    TransactionBuilder begin_transaction(NamespaceId ns) {
        return TransactionBuilder(ns);
    }

    void submit_transaction(Transaction tx) {
        m_conflict_detector->track(tx);
        m_transaction_queue->enqueue(std::move(tx));
    }

    std::optional<Transaction> process_next_transaction() {
        return m_transaction_queue->dequeue();
    }

    ValidationResult validate_transaction(const Transaction& tx) const {
        const Namespace* ns = m_namespaces->get(tx.namespace_id());
        if (!ns) {
            return ValidationResult::failed("Namespace not found");
        }

        PatchValidator validator(*m_schemas);
        return validator.validate_batch(tx.patches(), ns->permissions());
    }

    std::vector<Conflict> check_conflicts() const {
        return m_conflict_detector->detect();
    }

    std::optional<Conflict> check_transaction_conflict(const Transaction& tx) const {
        return m_conflict_detector->check(tx);
    }

    PatchBatch optimize_batch(const PatchBatch& batch) {
        return m_optimizer->optimize(batch);
    }

    PatchBatch deduplicate_batch(const PatchBatch& batch) {
        return m_deduplicator->deduplicate(batch);
    }

    PatchBatch process_batch(const PatchBatch& batch) {
        PatchBatch deduped = m_deduplicator->deduplicate(batch);
        return m_optimizer->optimize(deduped);
    }

    SnapshotId create_snapshot(NamespaceId ns, std::string description = "") {
        return m_snapshot_manager->create(ns, std::move(description));
    }

    void publish_patch(Patch patch, NamespaceId ns, TransactionId tx = TransactionId::invalid()) {
        m_patch_bus->publish(std::move(patch), ns, tx);
        m_async_bus->publish(Patch(patch), ns, tx);
    }

    void publish_batch(const PatchBatch& batch, NamespaceId ns, TransactionId tx = TransactionId::invalid()) {
        m_patch_bus->publish_batch(batch, ns, tx);
        m_async_bus->publish_batch(batch, ns, tx);
    }

    SubscriptionId subscribe(PatchFilter filter, PatchBus::Callback callback) {
        return m_patch_bus->subscribe(std::move(filter), std::move(callback));
    }

    void unsubscribe(SubscriptionId id) {
        m_patch_bus->unsubscribe(id);
    }

    void clear_conflict_tracking() {
        m_conflict_detector->clear();
    }

    void shutdown() {
        m_patch_bus->shutdown();
        m_async_bus->shutdown();
        m_transaction_queue->clear();
        m_conflict_detector->clear();
    }

    std::vector<std::uint8_t> snapshot_state() const {
        BinaryWriter writer;

        writer.write_u32(IR_MAGIC);
        writer.write_u32(IR_VERSION_MAJOR);
        writer.write_u32(IR_VERSION_MINOR);
        writer.write_u32(IR_VERSION_PATCH);

        auto ns_data = serialize_namespace_registry(*m_namespaces);
        writer.write_bytes(ns_data);

        auto schema_data = serialize_schema_registry(*m_schemas);
        writer.write_bytes(schema_data);

        writer.write_u64(m_next_transaction_id);

        writer.write_u64(static_cast<std::uint64_t>(m_patch_bus->sequence_number()));

        return writer.take();
    }

    bool restore_state(const std::vector<std::uint8_t>& data) {
        if (data.size() < 20) {
            return false;
        }

        BinaryReader reader(data);

        std::uint32_t magic = reader.read_u32();
        if (magic != IR_MAGIC) {
            return false;
        }

        std::uint32_t major = reader.read_u32();
        if (major != IR_VERSION_MAJOR) {
            return false;
        }

        reader.read_u32();
        reader.read_u32();

        auto ns_data = reader.read_bytes();
        auto ns_opt = deserialize_namespace_registry(ns_data);
        if (!ns_opt) {
            return false;
        }
        m_namespaces = std::make_shared<NamespaceRegistry>(std::move(*ns_opt));

        auto schema_data = reader.read_bytes();
        auto schema_opt = deserialize_schema_registry(schema_data);
        if (!schema_opt) {
            return false;
        }
        m_schemas = std::make_shared<SchemaRegistry>(std::move(*schema_opt));

        m_next_transaction_id = reader.read_u64();

        reader.read_u64();

        if (!reader.valid()) {
            return false;
        }

        m_patch_bus = std::make_shared<PatchBus>();
        m_async_bus = std::make_shared<AsyncPatchBus>();
        m_optimizer = std::make_shared<BatchOptimizer>();
        m_deduplicator = std::make_shared<PatchDeduplicator>();
        m_transaction_queue = std::make_shared<TransactionQueue>();
        m_conflict_detector = std::make_shared<ConflictDetector>();

        return true;
    }

private:
    std::shared_ptr<NamespaceRegistry> m_namespaces;
    std::shared_ptr<SchemaRegistry> m_schemas;
    std::shared_ptr<PatchBus> m_patch_bus;
    std::shared_ptr<AsyncPatchBus> m_async_bus;
    std::shared_ptr<BatchOptimizer> m_optimizer;
    std::shared_ptr<PatchDeduplicator> m_deduplicator;
    std::shared_ptr<TransactionQueue> m_transaction_queue;
    std::shared_ptr<ConflictDetector> m_conflict_detector;
    std::shared_ptr<SnapshotManager> m_snapshot_manager;
    std::uint64_t m_next_transaction_id;
};

class HotReloadableIRSystem : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadableIRSystem()
        : m_system(std::make_shared<IRSystem>()) {}

    explicit HotReloadableIRSystem(std::shared_ptr<IRSystem> system)
        : m_system(std::move(system)) {}

    IRSystem& system() {
        if (!m_system) {
            m_system = std::make_shared<IRSystem>();
        }
        return *m_system;
    }

    const IRSystem& system() const {
        return *m_system;
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_system) {
            return void_core::Err<void_core::HotReloadSnapshot>("IRSystem is null");
        }

        auto data = m_system->snapshot_state();

        void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(HotReloadableIRSystem)),
            "HotReloadableIRSystem",
            current_version()
        );

        snap.with_metadata("namespace_count", std::to_string(m_system->namespaces().size()));
        snap.with_metadata("schema_count", std::to_string(m_system->schemas().size()));

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadableIRSystem>()) {
            return void_core::Err("Type mismatch in HotReloadableIRSystem restore");
        }

        m_system = std::make_shared<IRSystem>();

        if (!m_system->restore_state(snap.data)) {
            return void_core::Err("Failed to restore IRSystem state");
        }

        return void_core::Ok();
    }

    bool is_compatible(const void_core::Version& new_version) const override {
        return new_version.major() == MAJOR_VERSION;
    }

    void_core::Result<void> prepare_reload() override {
        if (m_system) {
            m_system->shutdown();
        }
        return void_core::Ok();
    }

    void_core::Result<void> finish_reload() override {
        if (!m_system) {
            m_system = std::make_shared<IRSystem>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadableIRSystem";
    }

private:
    std::shared_ptr<IRSystem> m_system;
};

std::unique_ptr<IRSystem> create_ir_system() {
    return std::make_unique<IRSystem>();
}

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_ir_system() {
    return std::make_unique<HotReloadableIRSystem>();
}

std::unique_ptr<void_core::HotReloadable> wrap_ir_system(std::shared_ptr<IRSystem> system) {
    return std::make_unique<HotReloadableIRSystem>(std::move(system));
}

} // namespace void_ir
