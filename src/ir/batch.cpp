/// @file batch.cpp
/// @brief BatchOptimizer hot-reload implementation for void_ir

#include <void_engine/ir/batch.hpp>
#include <void_engine/ir/snapshot.hpp>
#include <void_engine/core/hot_reload.hpp>

namespace void_ir {

namespace {
    constexpr std::uint32_t BOPT_MAGIC = 0x424F5054;  // "BOPT"
    constexpr std::uint32_t BOPT_VERSION = 1;

    constexpr std::uint32_t BDED_MAGIC = 0x42444544;  // "BDED"
    constexpr std::uint32_t BDED_VERSION = 1;
}

std::vector<std::uint8_t> serialize_optimizer_options(const BatchOptimizer::Options& options) {
    BinaryWriter writer;

    writer.write_u32(BOPT_MAGIC);
    writer.write_u32(BOPT_VERSION);

    writer.write_bool(options.merge_consecutive);
    writer.write_bool(options.eliminate_contradictions);
    writer.write_bool(options.sort_for_efficiency);
    writer.write_bool(options.coalesce_field_patches);
    writer.write_bool(options.remove_redundant);

    return writer.take();
}

std::optional<BatchOptimizer::Options> deserialize_optimizer_options(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 13) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != BOPT_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != BOPT_VERSION) {
        return std::nullopt;
    }

    BatchOptimizer::Options options;
    options.merge_consecutive = reader.read_bool();
    options.eliminate_contradictions = reader.read_bool();
    options.sort_for_efficiency = reader.read_bool();
    options.coalesce_field_patches = reader.read_bool();
    options.remove_redundant = reader.read_bool();

    if (!reader.valid()) {
        return std::nullopt;
    }

    return options;
}

std::vector<std::uint8_t> serialize_optimization_stats(const OptimizationStats& stats) {
    BinaryWriter writer;

    writer.write_u64(static_cast<std::uint64_t>(stats.original_count));
    writer.write_u64(static_cast<std::uint64_t>(stats.optimized_count));
    writer.write_u64(static_cast<std::uint64_t>(stats.merged_count));
    writer.write_u64(static_cast<std::uint64_t>(stats.eliminated_count));
    writer.write_u64(static_cast<std::uint64_t>(stats.reordered_count));

    return writer.take();
}

OptimizationStats deserialize_optimization_stats(BinaryReader& reader) {
    OptimizationStats stats;

    stats.original_count = static_cast<std::size_t>(reader.read_u64());
    stats.optimized_count = static_cast<std::size_t>(reader.read_u64());
    stats.merged_count = static_cast<std::size_t>(reader.read_u64());
    stats.eliminated_count = static_cast<std::size_t>(reader.read_u64());
    stats.reordered_count = static_cast<std::size_t>(reader.read_u64());

    return stats;
}

class HotReloadableBatchOptimizer : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadableBatchOptimizer()
        : m_optimizer(std::make_shared<BatchOptimizer>()) {}

    explicit HotReloadableBatchOptimizer(BatchOptimizer::Options options)
        : m_optimizer(std::make_shared<BatchOptimizer>(options)) {}

    explicit HotReloadableBatchOptimizer(std::shared_ptr<BatchOptimizer> optimizer)
        : m_optimizer(std::move(optimizer)) {}

    BatchOptimizer& optimizer() {
        if (!m_optimizer) {
            m_optimizer = std::make_shared<BatchOptimizer>();
        }
        return *m_optimizer;
    }

    const BatchOptimizer& optimizer() const {
        return *m_optimizer;
    }

    PatchBatch optimize(const PatchBatch& input) {
        return optimizer().optimize(input);
    }

    const OptimizationStats& stats() const {
        return m_optimizer->stats();
    }

    const BatchOptimizer::Options& options() const {
        return m_optimizer->options();
    }

    void set_options(BatchOptimizer::Options options) {
        m_optimizer->set_options(options);
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_optimizer) {
            return void_core::Err<void_core::HotReloadSnapshot>("BatchOptimizer is null");
        }

        auto data = serialize_optimizer_options(m_optimizer->options());

        void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(HotReloadableBatchOptimizer)),
            "HotReloadableBatchOptimizer",
            current_version()
        );

        snap.with_metadata("merge_consecutive",
            m_optimizer->options().merge_consecutive ? "true" : "false");
        snap.with_metadata("eliminate_contradictions",
            m_optimizer->options().eliminate_contradictions ? "true" : "false");

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadableBatchOptimizer>()) {
            return void_core::Err("Type mismatch in HotReloadableBatchOptimizer restore");
        }

        auto options = deserialize_optimizer_options(snap.data);
        if (!options) {
            return void_core::Err("Failed to deserialize BatchOptimizer options");
        }

        m_optimizer = std::make_shared<BatchOptimizer>(*options);

        return void_core::Ok();
    }

    bool is_compatible(const void_core::Version& new_version) const override {
        return new_version.major == MAJOR_VERSION;
    }

    void_core::Result<void> prepare_reload() override {
        return void_core::Ok();
    }

    void_core::Result<void> finish_reload() override {
        if (!m_optimizer) {
            m_optimizer = std::make_shared<BatchOptimizer>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadableBatchOptimizer";
    }

private:
    std::shared_ptr<BatchOptimizer> m_optimizer;
};

std::vector<std::uint8_t> serialize_deduplicator_state(std::size_t removed_count) {
    BinaryWriter writer;

    writer.write_u32(BDED_MAGIC);
    writer.write_u32(BDED_VERSION);
    writer.write_u64(static_cast<std::uint64_t>(removed_count));

    return writer.take();
}

std::optional<std::size_t> deserialize_deduplicator_state(const std::vector<std::uint8_t>& data) {
    if (data.size() < 16) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != BDED_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != BDED_VERSION) {
        return std::nullopt;
    }

    std::size_t removed_count = static_cast<std::size_t>(reader.read_u64());

    if (!reader.valid()) {
        return std::nullopt;
    }

    return removed_count;
}

class HotReloadablePatchDeduplicator : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadablePatchDeduplicator()
        : m_deduplicator(std::make_shared<PatchDeduplicator>()) {}

    explicit HotReloadablePatchDeduplicator(std::shared_ptr<PatchDeduplicator> dedup)
        : m_deduplicator(std::move(dedup)) {}

    PatchDeduplicator& deduplicator() {
        if (!m_deduplicator) {
            m_deduplicator = std::make_shared<PatchDeduplicator>();
        }
        return *m_deduplicator;
    }

    const PatchDeduplicator& deduplicator() const {
        return *m_deduplicator;
    }

    PatchBatch deduplicate(const PatchBatch& input) {
        return deduplicator().deduplicate(input);
    }

    std::size_t removed_count() const {
        return m_deduplicator->removed_count();
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_deduplicator) {
            return void_core::Err<void_core::HotReloadSnapshot>("PatchDeduplicator is null");
        }

        auto data = serialize_deduplicator_state(m_deduplicator->removed_count());

        void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(HotReloadablePatchDeduplicator)),
            "HotReloadablePatchDeduplicator",
            current_version()
        );

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadablePatchDeduplicator>()) {
            return void_core::Err("Type mismatch in HotReloadablePatchDeduplicator restore");
        }

        m_deduplicator = std::make_shared<PatchDeduplicator>();

        return void_core::Ok();
    }

    bool is_compatible(const void_core::Version& new_version) const override {
        return new_version.major == MAJOR_VERSION;
    }

    void_core::Result<void> prepare_reload() override {
        return void_core::Ok();
    }

    void_core::Result<void> finish_reload() override {
        if (!m_deduplicator) {
            m_deduplicator = std::make_shared<PatchDeduplicator>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadablePatchDeduplicator";
    }

private:
    std::shared_ptr<PatchDeduplicator> m_deduplicator;
};

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_batch_optimizer() {
    return std::make_unique<HotReloadableBatchOptimizer>();
}

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_batch_optimizer(
    BatchOptimizer::Options options) {
    return std::make_unique<HotReloadableBatchOptimizer>(options);
}

std::unique_ptr<void_core::HotReloadable> wrap_batch_optimizer(
    std::shared_ptr<BatchOptimizer> optimizer) {
    return std::make_unique<HotReloadableBatchOptimizer>(std::move(optimizer));
}

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_deduplicator() {
    return std::make_unique<HotReloadablePatchDeduplicator>();
}

std::unique_ptr<void_core::HotReloadable> wrap_deduplicator(
    std::shared_ptr<PatchDeduplicator> dedup) {
    return std::make_unique<HotReloadablePatchDeduplicator>(std::move(dedup));
}

OptimizationStats aggregate_stats(const std::vector<OptimizationStats>& stats_list) {
    OptimizationStats result;

    for (const auto& stats : stats_list) {
        result += stats;
    }

    return result;
}

double calculate_total_reduction(const std::vector<OptimizationStats>& stats_list) {
    std::size_t total_original = 0;
    std::size_t total_optimized = 0;

    for (const auto& stats : stats_list) {
        total_original += stats.original_count;
        total_optimized += stats.optimized_count;
    }

    if (total_original == 0) {
        return 0.0;
    }

    return 100.0 * (1.0 - static_cast<double>(total_optimized) /
                          static_cast<double>(total_original));
}

} // namespace void_ir
