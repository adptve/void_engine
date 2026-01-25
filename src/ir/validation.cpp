/// @file validation.cpp
/// @brief Schema validation hot-reload implementation for void_ir

#include <void_engine/ir/validation.hpp>
#include <void_engine/ir/snapshot.hpp>
#include <void_engine/core/hot_reload.hpp>

namespace void_ir {

namespace {
    constexpr std::uint32_t SREG_MAGIC = 0x53524547;  // "SREG"
    constexpr std::uint32_t SREG_VERSION = 1;

    constexpr std::uint32_t CSCH_MAGIC = 0x43534348;  // "CSCH"
    constexpr std::uint32_t CSCH_VERSION = 1;
}

void serialize_numeric_range(BinaryWriter& writer, const std::optional<NumericRange>& range) {
    writer.write_bool(range.has_value());
    if (range) {
        writer.write_bool(range->min.has_value());
        if (range->min) {
            writer.write_f64(*range->min);
        }
        writer.write_bool(range->max.has_value());
        if (range->max) {
            writer.write_f64(*range->max);
        }
    }
}

std::optional<NumericRange> deserialize_numeric_range(BinaryReader& reader) {
    if (!reader.read_bool()) {
        return std::nullopt;
    }

    NumericRange range;
    if (reader.read_bool()) {
        range.min = reader.read_f64();
    }
    if (reader.read_bool()) {
        range.max = reader.read_f64();
    }
    return range;
}

void serialize_string_constraint(BinaryWriter& writer, const std::optional<StringConstraint>& constraint) {
    writer.write_bool(constraint.has_value());
    if (constraint) {
        writer.write_bool(constraint->min_length.has_value());
        if (constraint->min_length) {
            writer.write_u64(static_cast<std::uint64_t>(*constraint->min_length));
        }
        writer.write_bool(constraint->max_length.has_value());
        if (constraint->max_length) {
            writer.write_u64(static_cast<std::uint64_t>(*constraint->max_length));
        }
        writer.write_bool(constraint->pattern.has_value());
        if (constraint->pattern) {
            writer.write_string(*constraint->pattern);
        }
    }
}

std::optional<StringConstraint> deserialize_string_constraint(BinaryReader& reader) {
    if (!reader.read_bool()) {
        return std::nullopt;
    }

    StringConstraint constraint;
    if (reader.read_bool()) {
        constraint.min_length = static_cast<std::size_t>(reader.read_u64());
    }
    if (reader.read_bool()) {
        constraint.max_length = static_cast<std::size_t>(reader.read_u64());
    }
    if (reader.read_bool()) {
        constraint.pattern = reader.read_string();
    }
    return constraint;
}

void serialize_array_constraint(BinaryWriter& writer, const std::optional<ArrayConstraint>& constraint) {
    writer.write_bool(constraint.has_value());
    if (constraint) {
        writer.write_bool(constraint->min_length.has_value());
        if (constraint->min_length) {
            writer.write_u64(static_cast<std::uint64_t>(*constraint->min_length));
        }
        writer.write_bool(constraint->max_length.has_value());
        if (constraint->max_length) {
            writer.write_u64(static_cast<std::uint64_t>(*constraint->max_length));
        }
        writer.write_bool(constraint->element_type.has_value());
        if (constraint->element_type) {
            writer.write_u8(static_cast<std::uint8_t>(*constraint->element_type));
        }
    }
}

std::optional<ArrayConstraint> deserialize_array_constraint(BinaryReader& reader) {
    if (!reader.read_bool()) {
        return std::nullopt;
    }

    ArrayConstraint constraint;
    if (reader.read_bool()) {
        constraint.min_length = static_cast<std::size_t>(reader.read_u64());
    }
    if (reader.read_bool()) {
        constraint.max_length = static_cast<std::size_t>(reader.read_u64());
    }
    if (reader.read_bool()) {
        constraint.element_type = static_cast<FieldType>(reader.read_u8());
    }
    return constraint;
}

void serialize_field_descriptor(BinaryWriter& writer, const FieldDescriptor& field) {
    writer.write_string(field.name);
    writer.write_u8(static_cast<std::uint8_t>(field.type));
    writer.write_bool(field.required);
    writer.write_bool(field.nullable);

    serialize_value(writer, field.default_value);

    serialize_numeric_range(writer, field.numeric_range);
    serialize_string_constraint(writer, field.string_constraint);
    serialize_array_constraint(writer, field.array_constraint);

    writer.write_u32(static_cast<std::uint32_t>(field.enum_values.size()));
    for (const auto& val : field.enum_values) {
        writer.write_string(val);
    }
}

FieldDescriptor deserialize_field_descriptor(BinaryReader& reader) {
    FieldDescriptor field;

    field.name = reader.read_string();
    field.type = static_cast<FieldType>(reader.read_u8());
    field.required = reader.read_bool();
    field.nullable = reader.read_bool();

    field.default_value = deserialize_value(reader);

    field.numeric_range = deserialize_numeric_range(reader);
    field.string_constraint = deserialize_string_constraint(reader);
    field.array_constraint = deserialize_array_constraint(reader);

    std::uint32_t enum_count = reader.read_u32();
    field.enum_values.reserve(enum_count);
    for (std::uint32_t i = 0; i < enum_count; ++i) {
        field.enum_values.push_back(reader.read_string());
    }

    return field;
}

void serialize_component_schema(BinaryWriter& writer, const ComponentSchema& schema) {
    writer.write_u32(CSCH_MAGIC);
    writer.write_u32(CSCH_VERSION);

    writer.write_string(schema.type_name());

    const auto& fields = schema.fields();
    writer.write_u32(static_cast<std::uint32_t>(fields.size()));
    for (const auto& field : fields) {
        serialize_field_descriptor(writer, field);
    }
}

std::optional<ComponentSchema> deserialize_component_schema(BinaryReader& reader) {
    std::uint32_t magic = reader.read_u32();
    if (magic != CSCH_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != CSCH_VERSION) {
        return std::nullopt;
    }

    std::string type_name = reader.read_string();
    ComponentSchema schema(std::move(type_name));

    std::uint32_t field_count = reader.read_u32();
    for (std::uint32_t i = 0; i < field_count; ++i) {
        schema.field(deserialize_field_descriptor(reader));
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return schema;
}

std::vector<std::uint8_t> serialize_schema_registry(const SchemaRegistry& registry) {
    BinaryWriter writer;

    writer.write_u32(SREG_MAGIC);
    writer.write_u32(SREG_VERSION);

    auto type_names = registry.type_names();
    writer.write_u32(static_cast<std::uint32_t>(type_names.size()));

    for (const auto& name : type_names) {
        const ComponentSchema* schema = registry.get(name);
        if (schema) {
            serialize_component_schema(writer, *schema);
        }
    }

    return writer.take();
}

std::optional<SchemaRegistry> deserialize_schema_registry(const std::vector<std::uint8_t>& data) {
    if (data.size() < 12) {
        return std::nullopt;
    }

    BinaryReader reader(data);

    std::uint32_t magic = reader.read_u32();
    if (magic != SREG_MAGIC) {
        return std::nullopt;
    }

    std::uint32_t version = reader.read_u32();
    if (version != SREG_VERSION) {
        return std::nullopt;
    }

    std::uint32_t schema_count = reader.read_u32();

    SchemaRegistry registry;

    for (std::uint32_t i = 0; i < schema_count; ++i) {
        auto schema_opt = deserialize_component_schema(reader);
        if (!schema_opt) {
            return std::nullopt;
        }
        registry.register_schema(std::move(*schema_opt));
    }

    if (!reader.valid()) {
        return std::nullopt;
    }

    return registry;
}

class HotReloadableSchemaRegistry : public void_core::HotReloadable {
public:
    static constexpr std::uint32_t MAJOR_VERSION = 1;
    static constexpr std::uint32_t MINOR_VERSION = 0;
    static constexpr std::uint32_t PATCH_VERSION = 0;

    HotReloadableSchemaRegistry()
        : m_registry(std::make_shared<SchemaRegistry>()) {}

    explicit HotReloadableSchemaRegistry(std::shared_ptr<SchemaRegistry> registry)
        : m_registry(std::move(registry)) {}

    SchemaRegistry& registry() {
        if (!m_registry) {
            m_registry = std::make_shared<SchemaRegistry>();
        }
        return *m_registry;
    }

    const SchemaRegistry& registry() const {
        return *m_registry;
    }

    void register_schema(ComponentSchema schema) {
        registry().register_schema(std::move(schema));
    }

    const ComponentSchema* get(std::string_view type_name) const {
        return m_registry->get(type_name);
    }

    bool has(std::string_view type_name) const {
        return m_registry->has(type_name);
    }

    ValidationResult validate_patch(const ComponentPatch& patch) const {
        return m_registry->validate_patch(patch);
    }

    std::vector<std::string> type_names() const {
        return m_registry->type_names();
    }

    std::size_t size() const {
        return m_registry->size();
    }

    void clear() {
        m_registry->clear();
    }

    void_core::Result<void_core::HotReloadSnapshot> snapshot() override {
        if (!m_registry) {
            return void_core::Err<void_core::HotReloadSnapshot>("SchemaRegistry is null");
        }

        auto data = serialize_schema_registry(*m_registry);

        void_core::HotReloadSnapshot snap(
            std::move(data),
            std::type_index(typeid(HotReloadableSchemaRegistry)),
            "HotReloadableSchemaRegistry",
            current_version()
        );

        snap.with_metadata("schema_count", std::to_string(m_registry->size()));

        return void_core::Ok(std::move(snap));
    }

    void_core::Result<void> restore(void_core::HotReloadSnapshot snap) override {
        if (!snap.is_type<HotReloadableSchemaRegistry>()) {
            return void_core::Err("Type mismatch in HotReloadableSchemaRegistry restore");
        }

        auto registry_opt = deserialize_schema_registry(snap.data);
        if (!registry_opt) {
            return void_core::Err("Failed to deserialize SchemaRegistry");
        }

        m_registry = std::make_shared<SchemaRegistry>(std::move(*registry_opt));

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
            m_registry = std::make_shared<SchemaRegistry>();
        }
        return void_core::Ok();
    }

    void_core::Version current_version() const override {
        return void_core::Version(MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
    }

    std::string type_name() const override {
        return "HotReloadableSchemaRegistry";
    }

private:
    std::shared_ptr<SchemaRegistry> m_registry;
};

std::unique_ptr<void_core::HotReloadable> create_hot_reloadable_schema_registry() {
    return std::make_unique<HotReloadableSchemaRegistry>();
}

std::unique_ptr<void_core::HotReloadable> wrap_schema_registry(
    std::shared_ptr<SchemaRegistry> registry) {
    return std::make_unique<HotReloadableSchemaRegistry>(std::move(registry));
}

std::vector<std::uint8_t> serialize_component_schema_binary(const ComponentSchema& schema) {
    BinaryWriter writer;
    serialize_component_schema(writer, schema);
    return writer.take();
}

std::optional<ComponentSchema> deserialize_component_schema_binary(
    const std::vector<std::uint8_t>& data) {
    if (data.size() < 8) {
        return std::nullopt;
    }

    BinaryReader reader(data);
    return deserialize_component_schema(reader);
}

ValidationResult validate_batch_with_schemas(
    const PatchBatch& batch,
    const SchemaRegistry& schemas,
    const NamespacePermissions& permissions) {

    PatchValidator validator(schemas);
    return validator.validate_batch(batch, permissions);
}

std::string format_validation_result(const ValidationResult& result) {
    if (result.valid) {
        return "Validation passed";
    }

    std::string output = "Validation failed with " +
        std::to_string(result.errors.size()) + " error(s):\n";

    for (const auto& error : result.errors) {
        output += "  - " + error.to_string() + "\n";
    }

    return output;
}

ComponentSchema create_transform_schema() {
    ComponentSchema schema("Transform");

    schema.field(FieldDescriptor::vec3("position").with_default(Value(Vec3{0, 0, 0})));
    schema.field(FieldDescriptor::vec4("rotation").with_default(Value(Vec4{0, 0, 0, 1})));
    schema.field(FieldDescriptor::vec3("scale").with_default(Value(Vec3{1, 1, 1})));

    return schema;
}

ComponentSchema create_camera_schema() {
    ComponentSchema schema("Camera");

    schema.field(FieldDescriptor::float_range("fov", 1.0, 180.0).with_default(Value(60.0)));
    schema.field(FieldDescriptor::float_range("near", 0.001, 100000.0).with_default(Value(0.1)));
    schema.field(FieldDescriptor::float_range("far", 0.001, 100000.0).with_default(Value(1000.0)));
    schema.field(FieldDescriptor::boolean("orthographic").with_default(Value(false)));
    schema.field(FieldDescriptor::floating("ortho_size").with_default(Value(10.0)));
    schema.field(FieldDescriptor::vec4("viewport").with_default(Value(Vec4{0, 0, 1, 1})));
    schema.field(FieldDescriptor::vec4("clear_color").with_default(Value(Vec4{0.1f, 0.1f, 0.1f, 1.0f})));
    schema.field(FieldDescriptor::floating("depth").with_default(Value(0.0)));
    schema.field(FieldDescriptor::boolean("active").with_default(Value(true)));

    return schema;
}

ComponentSchema create_renderable_schema() {
    ComponentSchema schema("Renderable");

    schema.field(FieldDescriptor::asset_ref("mesh"));
    schema.field(FieldDescriptor::asset_ref("material"));
    schema.field(FieldDescriptor::boolean("visible").with_default(Value(true)));
    schema.field(FieldDescriptor::boolean("cast_shadows").with_default(Value(true)));
    schema.field(FieldDescriptor::boolean("receive_shadows").with_default(Value(true)));
    schema.field(FieldDescriptor::integer_range("render_layer", 0, 31).with_default(Value(0)));

    return schema;
}

void register_standard_schemas(SchemaRegistry& registry) {
    registry.register_schema(create_transform_schema());
    registry.register_schema(create_camera_schema());
    registry.register_schema(create_renderable_schema());
}

} // namespace void_ir
