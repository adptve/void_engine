/// @file namespace.cpp
/// @brief Namespace hot-reload implementation for void_ir

#include <void_engine/ir/namespace.hpp>
#include <void_engine/ir/snapshot.hpp>
#include <void_engine/core/hot_reload.hpp>

namespace void_ir {

namespace {
    constexpr std::uint32_t NREG_MAGIC = 0x4E524547;  // "NREG"
    constexpr std::uint32_t NREG_VERSION = 1;

    constexpr std::uint32_t NSPC_MAGIC = 0x4E535043;  // "NSPC"
    constexpr std::uint32_t NSPC_VERSION = 1;
}

void serialize_namespace_permissions(BinaryWriter& writer, const NamespacePermissions& perms) {
    writer.write_bool(perms.can_create_entities);
    writer.write_bool(perms.can_delete_entities);
    writer.write_bool(perms.can_modify_components);
    writer.write_bool(perms.can_cross_namespace);
    writer.write_bool(perms.can_snapshot);
    writer.write_bool(perms.can_modify_layers);
    writer.write_bool(perms.can_modify_hierarchy);

    writer.write_u32(static_cast<std::uint32_t>(perms.allowed_components.size()));
    for (const auto& comp : perms.allowed_components) {
        writer.write_string(comp);
    }

    writer.write_u32(static_cast<std::uint32_t>(perms.blocked_components.size()));
    for (const auto& comp : perms.blocked_components) {
        writer.write_string(comp);
    }
}

NamespacePermissions deserialize_namespace_permissions(BinaryReader& reader) {
    NamespacePermissions perms;

    perms.can_create_entities = reader.read_bool();
    perms.can_delete_entities = reader.read_bool();
    perms.can_modify_components = reader.read_bool();
    perms.can_cross_namespace = reader.read_bool();
    perms.can_snapshot = reader.read_bool();
    perms.can_modify_layers = reader.read_bool();
    perms.can_modify_hierarchy = reader.read_bool();

    std::uint32_t allowed_count = reader.read_u32();
    perms.allowed_components.reserve(allowed_count);
    for (std::uint32_t i = 0; i < allowed_count; ++i) {
        perms.allowed_components.push_back(reader.read_string());
    }

    std::uint32_t blocked_count = reader.read_u32();
    perms.blocked_components.reserve(blocked_count);
    for (std::uint32_t i = 0; i < blocked_count; ++i) {
        perms.blocked_components.push_back(reader.read_string());
    }

    return perms;
}

void serialize_resource_limits(BinaryWriter& writer, const ResourceLimits& limits) {
    writer.write_u64(static_cast<std::uint64_t>(limits.max_entities));
    writer.write_u64(static_cast<std::uint64_t>(limits.max_components_per_entity));
    writer.write_u64(static_cast<std::uint64_t>(limits.max_memory_bytes));
    writer.write_u64(static_cast<std::uint64_t>(limits.max_pending_transactions));
    writer.write_u64(static_cast<std::uint64_t>(limits.max_snapshots));
}

ResourceLimits deserialize_resource_limits(BinaryReader& reader) {
    ResourceLimits limits;

    limits.max_entities = static_cast<std::size_t>(reader.read_u64());
    limits.max_components_per_entity = static_cast<std::size_t>(reader.read_u64());
    limits.max_memory_bytes = static_cast<std::size_t>(reader.read_u64());
    limits.max_pending_transactions = static_cast<std::size_t>(reader.read_u64());
    limits.max_snapshots = static_cast<std::size_t>(reader.read_u64());

    return limits;
}

void serialize_resource_usage(BinaryWriter& writer, const ResourceUsage& usage) {
    writer.write_u64(static_cast<std::uint64_t>(usage.entity_count));
    writer.write_u64(static_cast<std::uint64_t>(usage.component_count));
    writer.write_u64(static_cast<std::uint64_t>(usage.memory_bytes));
    writer.write_u64(static_cast<std::uint64_t>(usage.pending_transactions));
    writer.write_u64(static_cast<std::uint64_t>(usage.snapshot_count));
}

ResourceUsage deserialize_resource_usage(BinaryReader& reader) {
    ResourceUsage usage;

    usage.entity_count = static_cast<std::size_t>(reader.read_u64());
    usage.component_count = static_cast<std::size_t>(reader.read_u64());
    usage.memory_bytes = static_cast<std::size_t>(reader.read_u64());
    usage.pending_transactions = static_cast<std::size_t>(reader.read_u64());
    usage.snapshot_count = static_cast<std::size_t>(reader.read_u64());

    return usage;
}

void serialize_namespace(BinaryWriter& writer, const Namespace& ns) {
    writer.write_u32(NSPC_MAGIC);
    writer.write_u32(NSPC_VERSION);

    writer.write_u32(ns.id().value);
    writer.write_string(ns.name());
    writer.write_u64(ns.peek_next_entity_id());

    serialize_namespace_permissions(writer, ns.permissions());
    serialize_resource_limits(writer, ns.limits());
    serialize_resource_usage(writer, ns.usage());
}

std::optional<Namespace> deserialize_namespace(BinaryReader& reader) {
    std::uint32_t magic = reader.read_u32();
    if (magic != NSPC_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != NSPC_VERSION) {
        return std::nullopt;
    }

    NamespaceId id{reader.read_u32()};
    std::string name = reader.read_string();
    std::uint64_t next_entity_id = reader.read_u64();

    NamespacePermissions perms = deserialize_namespace_permissions(reader);
    ResourceLimits limits = deserialize_resource_limits(reader);
    ResourceUsage usage = deserialize_resource_usage(reader);

    if (!reader.valid()) {
        return std::nullopt;
    }

    Namespace ns(id, std::move(name), std::move(perms), limits);
    ns.update_usage(usage);

    for (std::uint64_t i = 1; i < next_entity_id; ++i) {
        ns.allocate_entity();
    }

    return ns;
}

std::vector<std::uint8_t> serialize_namespace_registry(const NamespaceRegistry& registry) {
    BinaryWriter writer;

    writer.write_u32(NREG_MAGIC);
    writer.write_u32(NREG_VERSION);

    writer.write_u32(static_cast<std::uint32_t>(registry.size()));

    for (std::size_t i = 0; i < registry.size(); ++i) {
        NamespaceId id{static_cast<std::uint32_t>(i)};
        const Namespace* ns = registry.get(id);
        if (ns) {
            serialize_namespace(writer, *ns);
        }
    }

    return writer.take();
}

std::optional<NamespaceRegistry> deserialize_namespace_registry(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 12) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != NREG_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != NREG_VERSION) {
        return std::nullopt;
    }

    std::uint32_t ns_count = reader.read_u32();

    NamespaceRegistry registry;

    for (std::uint32_t i = 0; i < ns_count; ++i) {
        auto ns_opt = deserialize_namespace(reader);
        if (!ns_opt) {
            return std::nullopt;
        }

        registry.create(ns_opt->name(), ns_opt->permissions(), ns_opt->limits());

        Namespace* ns = registry.get(NamespaceId{i});
        if (ns) {
            ns->update_usage(ns_opt->usage());
            std::uint64_t next_id = ns_opt->peek_next_entity_id();
            for (std::uint64_t j = 1; j < next_id; ++j) {
                ns->allocate_entity();
            }
        }
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return registry;
}

class HotReloadableNamespaceRegistry : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadableNamespaceRegistry()
        : m_registry(std::make_shared<NamespaceRegistry>()) {}

    explicit HotReloadableNamespaceRegistry(std::shared_ptr<NamespaceRegistry> registry)
        : m_registry(std::move(registry)) {}

    NamespaceRegistry& registry() {
        if (!m_registry) {
            m_registry = std::make_shared<NamespaceRegistry>();
        }
        return *m_registry;
    }

    const NamespaceRegistry& registry() const {
        return *m_registry;
    }

    NamespaceId create(std::string name) {
        return registry().create(std::move(name));
    }

    NamespaceId create(std::string name, NamespacePermissions perms, ResourceLimits limits) {
        return registry().create(std::move(name), std::move(perms), limits);
    }

    Namespace* get(NamespaceId id) {
        return registry().get(id);
    }

    const Namespace* get(NamespaceId id) const {
        return m_registry->get(id);
    }

    std::optional<NamespaceId> find_by_name(std::string_view name) const {
        return m_registry->find_by_name(name);
    }

    std::size_t size() const {
        return m_registry->size();
    }

    bool empty() const {
        return m_registry->empty();
    }

    void clear() {
        m_registry->clear();
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_registry) {
            return void_core::Err<void_core::HotReloadSnapshot>("NamespaceRegistry is null");
        }

        auto data = serialize_namespace_registry(*m_registry);

        void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(HotReloadableNamespaceRegistry)),
            "HotReloadableNamespaceRegistry",
            current_version()
        );

        snap.with_metadata("namespace_count", std::to_string(m_registry->size()));

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadableNamespaceRegistry>()) {
            return void_core::Err("Type mismatch in HotReloadableNamespaceRegistry restore");
        }

        auto registry_opt = deserialize_namespace_registry(snap.data);
        if (!registry_opt) {
            return void_core::Err("Failed to deserialize NamespaceRegistry");
        }

        m_registry = std::make_shared<NamespaceRegistry>(std::move(*registry_opt));

        return void_core::Ok();
    }

    bool is_compatible(const void_core::Version& new_version) const override {
        return new_version.major() == MAJOR_VERSION;
    }

    void_core::Result<void> prepare_reload() override {
        return void_core::Ok();
    }

    void_core::Result<void> finish_reload() override {
        if (!m_registry) {
            m_registry = std::make_shared<NamespaceRegistry>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadableNamespaceRegistry";
    }

private:
    std::shared_ptr<NamespaceRegistry> m_registry;
};

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_namespace_registry() {
    return std::make_unique<HotReloadableNamespaceRegistry>();
}

std::unique_ptr<void_core::HotReloadable> wrap_namespace_registry(
    std::shared_ptr<NamespaceRegistry> registry) {
    return std::make_unique<HotReloadableNamespaceRegistry>(std::move(registry));
}

std::vector<std::uint8_t> serialize_namespace_binary(const Namespace& ns) {
    BinaryWriter writer;
    serialize_namespace(writer, ns);
    return writer.take();
}

std::optional<Namespace> deserialize_namespace_binary(const std::vector<std::uint8_t>& data) {
    if (data.size() < 8) {
        return std::nullopt;
    }

    BinaryReader reader(data);
    return deserialize_namespace(reader);
}

std::vector<NamespaceId> collect_namespace_ids(const NamespaceRegistry& registry) {
    std::vector<NamespaceId> ids;
    ids.reserve(registry.size());

    for (std::size_t i = 0; i < registry.size(); ++i) {
        ids.push_back(NamespaceId{static_cast<std::uint32_t>(i)});
    }

    return ids;
}

std::vector<std::string> collect_namespace_names(const NamespaceRegistry& registry) {
    std::vector<std::string> names;
    names.reserve(registry.size());

    for (std::size_t i = 0; i < registry.size(); ++i) {
        const Namespace* ns = registry.get(NamespaceId{static_cast<std::uint32_t>(i)});
        if (ns) {
            names.push_back(ns->name());
        }
    }

    return names;
}

ResourceUsage total_resource_usage(const NamespaceRegistry& registry) {
    ResourceUsage total;

    for (std::size_t i = 0; i < registry.size(); ++i) {
        const Namespace* ns = registry.get(NamespaceId{static_cast<std::uint32_t>(i)});
        if (ns) {
            const auto& usage = ns->usage();
            total.entity_count += usage.entity_count;
            total.component_count += usage.component_count;
            total.memory_bytes += usage.memory_bytes;
            total.pending_transactions += usage.pending_transactions;
            total.snapshot_count += usage.snapshot_count;
        }
    }

    return total;
}

} // namespace void_ir
