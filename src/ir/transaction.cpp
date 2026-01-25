/// @file transaction.cpp
/// @brief Transaction hot-reload implementation for void_ir

#include <void_engine/ir/transaction.hpp>
#include <void_engine/ir/patch.hpp>
#include <void_engine/ir/snapshot.hpp>
#include <void_engine/core/hot_reload.hpp>

namespace void_ir {

// Forward declarations from patch.cpp
void serialize_patch(BinaryWriter& writer, const Patch& patch);
Patch deserialize_patch(BinaryReader& reader);

namespace {
    constexpr std::uint32_t TX_MAGIC = 0x54584E53;    // "TXNS"
    constexpr std::uint32_t TX_VERSION = 1;

    constexpr std::uint32_t TXQ_MAGIC = 0x54585155;   // "TXQU"
    constexpr std::uint32_t TXQ_VERSION = 1;

    constexpr std::uint32_t CFD_MAGIC = 0x43464454;   // "CFDT"
    constexpr std::uint32_t CFD_VERSION = 1;
}

void serialize_transaction_metadata(BinaryWriter& writer, const TransactionMetadata& meta) {
    writer.write_string(meta.description);
    writer.write_string(meta.source);
    writer.write_u8(static_cast<std::uint8_t>(meta.priority));
}

TransactionMetadata deserialize_transaction_metadata(BinaryReader& reader) {
    TransactionMetadata meta;
    meta.description = reader.read_string();
    meta.source = reader.read_string();
    meta.priority = static_cast<TransactionPriority>(reader.read_u8());
    meta.created_at = std::chrono::steady_clock::now();
    return meta;
}

void serialize_transaction(BinaryWriter& writer, const Transaction& tx) {
    writer.write_u32(TX_MAGIC);
    writer.write_u32(TX_VERSION);

    writer.write_u64(tx.id().value);
    writer.write_u32(tx.namespace_id().value);
    writer.write_u8(static_cast<std::uint8_t>(tx.state()));

    serialize_transaction_metadata(writer, tx.metadata());

    writer.write_u64(tx.frame());

    writer.write_u32(static_cast<std::uint32_t>(tx.dependencies().size()));
    for (TransactionId dep : tx.dependencies()) {
        writer.write_u64(dep.value);
    }

    const auto& patches = tx.patches();
    writer.write_u32(static_cast<std::uint32_t>(patches.size()));
    for (const auto& patch : patches) {
        serialize_patch(writer, patch);
    }

    auto rollback_snap = tx.rollback_snapshot();
    writer.write_bool(rollback_snap.has_value());
    if (rollback_snap) {
        writer.write_u64(rollback_snap->value);
    }

    writer.write_string(tx.error());
}

std::optional<Transaction> deserialize_transaction(BinaryReader& reader) {
    std::uint32_t magic = reader.read_u32();
    if (magic != TX_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != TX_VERSION) {
        return std::nullopt;
    }

    TransactionId id{reader.read_u64()};
    NamespaceId ns{reader.read_u32()};
    TransactionState state = static_cast<TransactionState>(reader.read_u8());

    Transaction tx(id, ns);

    TransactionMetadata meta = deserialize_transaction_metadata(reader);
    tx.metadata() = std::move(meta);

    std::uint64_t frame = reader.read_u64();
    tx.set_frame(frame);

    std::uint32_t dep_count = reader.read_u32();
    for (std::uint32_t i = 0; i < dep_count; ++i) {
        tx.add_dependency(TransactionId{reader.read_u64()});
    }

    std::uint32_t patch_count = reader.read_u32();
    for (std::uint32_t i = 0; i < patch_count; ++i) {
        tx.add_patch(deserialize_patch(reader));
    }

    bool has_rollback = reader.read_bool();
    if (has_rollback) {
        tx.set_rollback_snapshot(SnapshotId{reader.read_u64()});
    }

    std::string error = reader.read_string();

    if (!reader.valid()) {
        return std::nullopt;
    }

    if (state != TransactionState::Building) {
        tx.submit();

        if (state == TransactionState::Applying ||
            state == TransactionState::Committed ||
            state == TransactionState::RolledBack ||
            state == TransactionState::Failed) {
            tx.begin_apply();
        }

        if (state == TransactionState::Committed) {
            tx.commit();
        } else if (state == TransactionState::RolledBack) {
            tx.rollback();
        } else if (state == TransactionState::Failed) {
            tx.fail(error);
        }
    }

    return tx;
}

std::vector<std::uint8_t> serialize_transaction_binary(const Transaction& tx) {
    BinaryWriter writer;
    serialize_transaction(writer, tx);
    return writer.take();
}

std::optional<Transaction> deserialize_transaction_binary(const std::vector<std::uint8_t>& data) {
    if (data.size() < 8) {
        return std::nullopt;
    }

    BinaryReader reader(data);
    return deserialize_transaction(reader);
}

std::vector<std::uint8_t> serialize_transaction_queue(const TransactionQueue& queue) {
    BinaryWriter writer;

    writer.write_u32(TXQ_MAGIC);
    writer.write_u32(TXQ_VERSION);

    writer.write_u32(static_cast<std::uint32_t>(queue.size()));

    return writer.take();
}

std::optional<std::size_t> deserialize_transaction_queue_size(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 12) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != TXQ_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != TXQ_VERSION) {
        return std::nullopt;
    }

    std::uint32_t count = reader.read_u32();

    if (!reader.valid()) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(count);
}

void serialize_conflict(BinaryWriter& writer, const Conflict& conflict) {
    writer.write_u8(static_cast<std::uint8_t>(conflict.type));
    writer.write_u64(conflict.tx_a.value);
    writer.write_u64(conflict.tx_b.value);

    writer.write_bool(conflict.entity.has_value());
    if (conflict.entity) {
        writer.write_u32(conflict.entity->namespace_id.value);
        writer.write_u64(conflict.entity->entity_id);
    }

    writer.write_bool(conflict.component_type.has_value());
    if (conflict.component_type) {
        writer.write_string(*conflict.component_type);
    }

    writer.write_bool(conflict.layer.has_value());
    if (conflict.layer) {
        writer.write_u32(conflict.layer->value);
    }

    writer.write_bool(conflict.asset.has_value());
    if (conflict.asset) {
        writer.write_string(conflict.asset->path);
        writer.write_u64(conflict.asset->uuid);
    }
}

Conflict deserialize_conflict(BinaryReader& reader) {
    Conflict conflict;
    conflict.type = static_cast<ConflictType>(reader.read_u8());
    conflict.tx_a = TransactionId{reader.read_u64()};
    conflict.tx_b = TransactionId{reader.read_u64()};

    if (reader.read_bool()) {
        EntityRef entity;
        entity.namespace_id = NamespaceId{reader.read_u32()};
        entity.entity_id = reader.read_u64();
        conflict.entity = entity;
    }

    if (reader.read_bool()) {
        conflict.component_type = reader.read_string();
    }

    if (reader.read_bool()) {
        conflict.layer = LayerId{reader.read_u32()};
    }

    if (reader.read_bool()) {
        AssetRef asset;
        asset.path = reader.read_string();
        asset.uuid = reader.read_u64();
        conflict.asset = asset;
    }

    return conflict;
}

std::vector<std::uint8_t> serialize_conflict_detector(const ConflictDetector& detector) {
    BinaryWriter writer;

    writer.write_u32(CFD_MAGIC);
    writer.write_u32(CFD_VERSION);

    writer.write_u64(static_cast<std::uint64_t>(detector.entity_count()));
    writer.write_u64(static_cast<std::uint64_t>(detector.component_count()));

    return writer.take();
}

class HotReloadableTransactionQueue : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadableTransactionQueue()
        : m_queue(std::make_shared<TransactionQueue>()) {}

    explicit HotReloadableTransactionQueue(std::shared_ptr<TransactionQueue> queue)
        : m_queue(std::move(queue)) {}

    TransactionQueue& queue() {
        if (!m_queue) {
            m_queue = std::make_shared<TransactionQueue>();
        }
        return *m_queue;
    }

    const TransactionQueue& queue() const {
        return *m_queue;
    }

    void enqueue(Transaction tx) {
        queue().enqueue(std::move(tx));
    }

    std::optional<Transaction> dequeue() {
        return queue().dequeue();
    }

    const Transaction* peek() const {
        return m_queue->peek();
    }

    std::size_t size() const {
        return m_queue->size();
    }

    bool empty() const {
        return m_queue->empty();
    }

    void clear() {
        m_queue->clear();
    }

    std::size_t total_patch_count() const {
        return m_queue->total_patch_count();
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_queue) {
            return void_core::Err<void_core::HotReloadSnapshot>("TransactionQueue is null");
        }

        BinaryWriter writer;
        writer.write_u32(TXQ_MAGIC);
        writer.write_u32(TXQ_VERSION);

        std::vector<Transaction> transactions;
        TransactionQueue temp_queue;

        while (!m_queue->empty()) {
            auto tx = m_queue->dequeue();
            if (tx) {
                transactions.push_back(std::move(*tx));
            }
        }

        writer.write_u32(static_cast<std::uint32_t>(transactions.size()));

        for (const auto& tx : transactions) {
            serialize_transaction(writer, tx);
            m_queue->enqueue(Transaction(tx.id(), tx.namespace_id()));
        }

        for (auto& tx : transactions) {
            temp_queue.enqueue(std::move(tx));
        }

        while (!temp_queue.empty()) {
            auto tx = temp_queue.dequeue();
            if (tx) {
                m_queue->enqueue(std::move(*tx));
            }
        }

        void_core::HotReloadSnapshot snap(
            writer.take(),
            std::type_index(typeid(HotReloadableTransactionQueue)),
            "HotReloadableTransactionQueue",
            current_version()
        );

        snap.with_metadata("transaction_count", std::to_string(transactions.size()));

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadableTransactionQueue>()) {
            return void_core::Err("Type mismatch in HotReloadableTransactionQueue restore");
        }

        if (snap.data.size() < 12) {
            return void_core::Err("Invalid TransactionQueue snapshot data");
        }

        BinaryReader reader(snap.data);

        std::uint32_t magic = reader.read_u32();
        if (magic != TXQ_MAGIC) {
            return void_core::Err("Invalid TransactionQueue snapshot magic");
        }

        std::uint32_t version = reader.read_u32();
        if (version != TXQ_VERSION) {
            return void_core::Err("Incompatible TransactionQueue snapshot version");
        }

        std::uint32_t tx_count = reader.read_u32();

        m_queue = std::make_shared<TransactionQueue>();

        for (std::uint32_t i = 0; i < tx_count; ++i) {
            auto tx_opt = deserialize_transaction(reader);
            if (!tx_opt) {
                return void_core::Err("Failed to deserialize transaction");
            }
            m_queue->enqueue(std::move(*tx_opt));
        }

        if (!reader.valid()) {
            return void_core::Err("TransactionQueue snapshot data corrupted");
        }

        return void_core::Ok();
    }

    bool is_compatible(const void_core::Version& new_version) const override {
        return new_version.major == MAJOR_VERSION;
    }

    void_core::Result<void> prepare_reload() override {
        return void_core::Ok();
    }

    void_core::Result<void> finish_reload() override {
        if (!m_queue) {
            m_queue = std::make_shared<TransactionQueue>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadableTransactionQueue";
    }

private:
    std::shared_ptr<TransactionQueue> m_queue;
};

class HotReloadableConflictDetector : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadableConflictDetector()
        : m_detector(std::make_shared<ConflictDetector>()) {}

    explicit HotReloadableConflictDetector(std::shared_ptr<ConflictDetector> detector)
        : m_detector(std::move(detector)) {}

    ConflictDetector& detector() {
        if (!m_detector) {
            m_detector = std::make_shared<ConflictDetector>();
        }
        return *m_detector;
    }

    const ConflictDetector& detector() const {
        return *m_detector;
    }

    void track(const Transaction& tx) {
        detector().track(tx);
    }

    std::vector<Conflict> detect() const {
        return m_detector->detect();
    }

    std::optional<Conflict> check(const Transaction& tx) const {
        return m_detector->check(tx);
    }

    void clear() {
        m_detector->clear();
    }

    std::size_t entity_count() const {
        return m_detector->entity_count();
    }

    std::size_t component_count() const {
        return m_detector->component_count();
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_detector) {
            return void_core::Err<void_core::HotReloadSnapshot>("ConflictDetector is null");
        }

        auto data = serialize_conflict_detector(*m_detector);

        void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(HotReloadableConflictDetector)),
            "HotReloadableConflictDetector",
            current_version()
        );

        snap.with_metadata("entity_count", std::to_string(m_detector->entity_count()));
        snap.with_metadata("component_count", std::to_string(m_detector->component_count()));

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadableConflictDetector>()) {
            return void_core::Err("Type mismatch in HotReloadableConflictDetector restore");
        }

        m_detector = std::make_shared<ConflictDetector>();

        return void_core::Ok();
    }

    bool is_compatible(const void_core::Version& new_version) const override {
        return new_version.major == MAJOR_VERSION;
    }

    void_core::Result<void> prepare_reload() override {
        if (m_detector) {
            m_detector->clear();
        }
        return void_core::Ok();
    }

    void_core::Result<void> finish_reload() override {
        if (!m_detector) {
            m_detector = std::make_shared<ConflictDetector>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadableConflictDetector";
    }

private:
    std::shared_ptr<ConflictDetector> m_detector;
};

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_transaction_queue() {
    return std::make_unique<HotReloadableTransactionQueue>();
}

std::unique_ptr<void_core::HotReloadable> wrap_transaction_queue(
    std::shared_ptr<TransactionQueue> queue) {
    return std::make_unique<HotReloadableTransactionQueue>(std::move(queue));
}

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_conflict_detector() {
    return std::make_unique<HotReloadableConflictDetector>();
}

std::unique_ptr<void_core::HotReloadable> wrap_conflict_detector(
    std::shared_ptr<ConflictDetector> detector) {
    return std::make_unique<HotReloadableConflictDetector>(std::move(detector));
}

TransactionResult apply_transaction(Transaction& tx, const std::function<bool(const Patch&)>& handler) {
    if (tx.state() != TransactionState::Pending) {
        return TransactionResult::failed("Transaction not in Pending state");
    }

    tx.begin_apply();

    std::size_t applied = 0;
    std::size_t failed = 0;
    std::vector<std::size_t> failed_indices;

    const auto& patches = tx.patches();
    for (std::size_t i = 0; i < patches.size(); ++i) {
        bool success = handler(patches.patches()[i]);
        if (success) {
            ++applied;
        } else {
            ++failed;
            failed_indices.push_back(i);
        }
    }

    if (failed > 0) {
        tx.rollback();
        return TransactionResult::partial(applied, failed, std::move(failed_indices));
    }

    tx.commit();
    return TransactionResult::ok(applied);
}

bool transaction_affects_entity(const Transaction& tx, EntityRef entity) {
    for (const auto& patch : tx.patches()) {
        auto target = patch.target_entity();
        if (target && *target == entity) {
            return true;
        }
    }
    return false;
}

std::vector<EntityRef> collect_affected_entities(const Transaction& tx) {
    std::vector<EntityRef> entities;
    std::unordered_set<std::uint64_t> seen;

    for (const auto& patch : tx.patches()) {
        auto target = patch.target_entity();
        if (target && seen.insert(target->entity_id).second) {
            entities.push_back(*target);
        }
    }

    return entities;
}

std::vector<std::string> collect_affected_components(const Transaction& tx) {
    std::vector<std::string> components;
    std::unordered_set<std::string> seen;

    for (const auto& patch : tx.patches()) {
        if (const auto* cp = patch.try_as<ComponentPatch>()) {
            if (seen.insert(cp->component_type).second) {
                components.push_back(cp->component_type);
            }
        }
    }

    return components;
}

} // namespace void_ir
