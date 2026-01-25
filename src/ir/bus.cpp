/// @file bus.cpp
/// @brief PatchBus hot-reload implementation for void_ir

#include <void_engine/ir/bus.hpp>
#include <void_engine/ir/snapshot.hpp>
#include <void_engine/core/hot_reload.hpp>
#include <cstring>

namespace void_ir {

namespace {
    constexpr std::uint32_t PBUS_MAGIC = 0x50425553;   // "PBUS"
    constexpr std::uint32_t PBUS_VERSION = 1;

    constexpr std::uint32_t APBUS_MAGIC = 0x41504255;  // "APBU"
    constexpr std::uint32_t APBUS_VERSION = 1;
}

struct PatchBusState {
    std::uint64_t next_subscription_id = 0;
    std::size_t sequence_number = 0;
};

std::vector<std::uint8_t> serialize_patch_bus_state(const PatchBusState& state) {
    BinaryWriter writer;

    writer.write_u32(PBUS_MAGIC);
    writer.write_u32(PBUS_VERSION);
    writer.write_u64(state.next_subscription_id);
    writer.write_u64(static_cast<std::uint64_t>(state.sequence_number));

    return writer.take();
}

std::optional<PatchBusState> deserialize_patch_bus_state(const std::vector<std::uint8_t>& data) {
    if (data.size() < 24) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != PBUS_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != PBUS_VERSION) {
        return std::nullopt;
    }

    PatchBusState state;
    state.next_subscription_id = reader.read_u64();
    state.sequence_number = static_cast<std::size_t>(reader.read_u64());

    if (!reader.valid()) {
        return std::nullopt;
    }

    return state;
}

class HotReloadablePatchBus : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadablePatchBus() = default;

    explicit HotReloadablePatchBus(std::shared_ptr<PatchBus> bus)
        : m_bus(std::move(bus)) {}

    PatchBus& bus() {
        if (!m_bus) {
            m_bus = std::make_shared<PatchBus>();
        }
        return *m_bus;
    }

    const PatchBus& bus() const {
        return *m_bus;
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_bus) {
            return void_core::Err<void_core::HotReloadSnapshot>("PatchBus is null");
        }

        PatchBusState state;
        state.sequence_number = m_bus->sequence_number();

        auto data = serialize_patch_bus_state(state);

        void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(HotReloadablePatchBus)),
            "HotReloadablePatchBus",
            current_version()
        );

        snap.with_metadata("sequence_number", std::to_string(state.sequence_number));
        snap.with_metadata("subscription_count", std::to_string(m_bus->subscription_count()));

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadablePatchBus>()) {
            return void_core::Err("Type mismatch in HotReloadablePatchBus restore");
        }

        auto state = deserialize_patch_bus_state(snap.data);
        if (!state) {
            return void_core::Err("Failed to deserialize PatchBus state");
        }

        m_bus = std::make_shared<PatchBus>();

        return void_core::Ok();
    }

    bool is_compatible(const void_core::Version& new_version) const override {
        return new_version.major() == MAJOR_VERSION;
    }

    void_core::Result<void> prepare_reload() override {
        if (m_bus) {
            m_bus->shutdown();
        }
        return void_core::Ok();
    }

    void_core::Result<void> finish_reload() override {
        if (!m_bus) {
            m_bus = std::make_shared<PatchBus>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadablePatchBus";
    }

private:
    std::shared_ptr<PatchBus> m_bus;
};

struct AsyncPatchBusState {
    std::size_t sequence_number = 0;
    std::vector<PatchEvent> pending_events;
};

std::vector<std::uint8_t> serialize_async_patch_bus_state(const AsyncPatchBusState& state) {
    BinaryWriter writer;

    writer.write_u32(APBUS_MAGIC);
    writer.write_u32(APBUS_VERSION);
    writer.write_u64(static_cast<std::uint64_t>(state.sequence_number));

    writer.write_u32(static_cast<std::uint32_t>(state.pending_events.size()));

    for (const auto& event : state.pending_events) {
        serialize_patch(writer, event.patch);
        writer.write_u32(event.namespace_id.value);
        writer.write_u64(event.transaction_id.value);
        writer.write_u64(static_cast<std::uint64_t>(event.sequence_number));
    }

    return writer.take();
}

std::optional<AsyncPatchBusState> deserialize_async_patch_bus_state(
    const std::vector<std::uint8_t>& data) {

    if (data.size() < 16) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != APBUS_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != APBUS_VERSION) {
        return std::nullopt;
    }

    AsyncPatchBusState state;
    state.sequence_number = static_cast<std::size_t>(reader.read_u64());

    std::uint32_t event_count = reader.read_u32();

    for (std::uint32_t i = 0; i < event_count; ++i) {
        Patch patch = deserialize_patch(reader);
        NamespaceId ns{reader.read_u32()};
        TransactionId tx{reader.read_u64()};
        std::size_t seq = static_cast<std::size_t>(reader.read_u64());

        state.pending_events.emplace_back(std::move(patch), ns, tx, seq);
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return state;
}

class HotReloadableAsyncPatchBus : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadableAsyncPatchBus() = default;

    explicit HotReloadableAsyncPatchBus(std::shared_ptr<AsyncPatchBus> bus)
        : m_bus(std::move(bus)) {}

    AsyncPatchBus& bus() {
        if (!m_bus) {
            m_bus = std::make_shared<AsyncPatchBus>();
        }
        return *m_bus;
    }

    const AsyncPatchBus& bus() const {
        return *m_bus;
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_bus) {
            return void_core::Err<void_core::HotReloadSnapshot>("AsyncPatchBus is null");
        }

        AsyncPatchBusState state;
        state.pending_events = m_bus->consume_all();

        auto data = serialize_async_patch_bus_state(state);

        void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(HotReloadableAsyncPatchBus)),
            "HotReloadableAsyncPatchBus",
            current_version()
        );

        snap.with_metadata("pending_count", std::to_string(state.pending_events.size()));

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadableAsyncPatchBus>()) {
            return void_core::Err("Type mismatch in HotReloadableAsyncPatchBus restore");
        }

        auto state = deserialize_async_patch_bus_state(snap.data);
        if (!state) {
            return void_core::Err("Failed to deserialize AsyncPatchBus state");
        }

        m_bus = std::make_shared<AsyncPatchBus>();

        for (auto& event : state->pending_events) {
            m_bus->publish(std::move(event.patch), event.namespace_id, event.transaction_id);
        }

        return void_core::Ok();
    }

    bool is_compatible(const void_core::Version& new_version) const override {
        return new_version.major() == MAJOR_VERSION;
    }

    void_core::Result<void> prepare_reload() override {
        return void_core::Ok();
    }

    void_core::Result<void> finish_reload() override {
        if (!m_bus) {
            m_bus = std::make_shared<AsyncPatchBus>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadableAsyncPatchBus";
    }

private:
    std::shared_ptr<AsyncPatchBus> m_bus;
};

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_patch_bus() {
    return std::make_unique<HotReloadablePatchBus>();
}

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_async_patch_bus() {
    return std::make_unique<HotReloadableAsyncPatchBus>();
}

std::unique_ptr<void_core::HotReloadable> wrap_patch_bus(std::shared_ptr<PatchBus> bus) {
    return std::make_unique<HotReloadablePatchBus>(std::move(bus));
}

std::unique_ptr<void_core::HotReloadable> wrap_async_patch_bus(std::shared_ptr<AsyncPatchBus> bus) {
    return std::make_unique<HotReloadableAsyncPatchBus>(std::move(bus));
}

} // namespace void_ir
