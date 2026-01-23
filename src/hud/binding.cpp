/// @file binding.cpp
/// @brief Implementation of data binding system for void_hud module

#include "void_engine/hud/binding.hpp"
#include "void_engine/hud/elements.hpp"

#include <algorithm>
#include <sstream>

namespace void_hud {

// =============================================================================
// SimpleDataSource
// =============================================================================

std::any SimpleDataSource::get_property(std::string_view path) const {
    std::string path_str(path);
    auto it = m_properties.find(path_str);
    return it != m_properties.end() ? it->second : std::any{};
}

bool SimpleDataSource::set_property(std::string_view path, const std::any& value) {
    std::string path_str(path);
    m_properties[path_str] = value;
    notify_property_changed(path_str);
    return true;
}

bool SimpleDataSource::has_property(std::string_view path) const {
    return m_properties.contains(std::string(path));
}

void SimpleDataSource::subscribe(std::string_view path, std::function<void(const std::any&)> callback) {
    std::string path_str(path);
    m_subscribers[path_str].push_back(std::move(callback));
}

void SimpleDataSource::unsubscribe(std::string_view path) {
    m_subscribers.erase(std::string(path));
}

void SimpleDataSource::notify_property_changed(std::string_view path) {
    std::string path_str(path);
    auto it = m_subscribers.find(path_str);
    if (it != m_subscribers.end()) {
        auto value = get_property(path);
        for (auto& callback : it->second) {
            if (callback) {
                callback(value);
            }
        }
    }
}

// =============================================================================
// StringFormatConverter
// =============================================================================

StringFormatConverter::StringFormatConverter(const std::string& format)
    : m_format(format) {}

std::any StringFormatConverter::convert(const std::any& value) const {
    if (!value.has_value()) {
        return std::string{};
    }

    std::string result = m_format;

    try {
        std::string value_str;

        if (value.type() == typeid(int)) {
            value_str = std::to_string(std::any_cast<int>(value));
        } else if (value.type() == typeid(float)) {
            std::ostringstream oss;
            oss << std::any_cast<float>(value);
            value_str = oss.str();
        } else if (value.type() == typeid(double)) {
            std::ostringstream oss;
            oss << std::any_cast<double>(value);
            value_str = oss.str();
        } else if (value.type() == typeid(bool)) {
            value_str = std::any_cast<bool>(value) ? "true" : "false";
        } else if (value.type() == typeid(std::string)) {
            value_str = std::any_cast<std::string>(value);
        }

        auto pos = result.find("{}");
        if (pos != std::string::npos) {
            result.replace(pos, 2, value_str);
        }
    } catch (...) {
        // Return format on error
    }

    return result;
}

std::any StringFormatConverter::convert_back(const std::any& value) const {
    // String to original type conversion not implemented
    return value;
}

// =============================================================================
// ClampConverter
// =============================================================================

ClampConverter::ClampConverter(float min_val, float max_val)
    : m_min(min_val), m_max(max_val) {}

std::any ClampConverter::convert(const std::any& value) const {
    if (!value.has_value()) {
        return m_min;
    }

    float val = 0;
    try {
        if (value.type() == typeid(int)) {
            val = static_cast<float>(std::any_cast<int>(value));
        } else if (value.type() == typeid(float)) {
            val = std::any_cast<float>(value);
        } else if (value.type() == typeid(double)) {
            val = static_cast<float>(std::any_cast<double>(value));
        }
    } catch (...) {
        return m_min;
    }

    return std::clamp(val, m_min, m_max);
}

std::any ClampConverter::convert_back(const std::any& value) const {
    return convert(value);
}

// =============================================================================
// NormalizeConverter
// =============================================================================

NormalizeConverter::NormalizeConverter(float min_val, float max_val)
    : m_min(min_val), m_max(max_val) {}

std::any NormalizeConverter::convert(const std::any& value) const {
    if (!value.has_value()) {
        return 0.0f;
    }

    float val = 0;
    try {
        if (value.type() == typeid(int)) {
            val = static_cast<float>(std::any_cast<int>(value));
        } else if (value.type() == typeid(float)) {
            val = std::any_cast<float>(value);
        } else if (value.type() == typeid(double)) {
            val = static_cast<float>(std::any_cast<double>(value));
        }
    } catch (...) {
        return 0.0f;
    }

    float range = m_max - m_min;
    if (range <= 0) return 0.0f;

    return (val - m_min) / range;
}

std::any NormalizeConverter::convert_back(const std::any& value) const {
    if (!value.has_value()) {
        return m_min;
    }

    float normalized = 0;
    try {
        normalized = std::any_cast<float>(value);
    } catch (...) {
        return m_min;
    }

    return m_min + normalized * (m_max - m_min);
}

// =============================================================================
// ColorInterpolateConverter
// =============================================================================

ColorInterpolateConverter::ColorInterpolateConverter(const Color& from, const Color& to)
    : m_from(from), m_to(to) {}

std::any ColorInterpolateConverter::convert(const std::any& value) const {
    if (!value.has_value()) {
        return m_from;
    }

    float t = 0;
    try {
        if (value.type() == typeid(float)) {
            t = std::any_cast<float>(value);
        } else if (value.type() == typeid(double)) {
            t = static_cast<float>(std::any_cast<double>(value));
        }
    } catch (...) {
        return m_from;
    }

    t = std::clamp(t, 0.0f, 1.0f);
    return m_from.lerp(m_to, t);
}

std::any ColorInterpolateConverter::convert_back(const std::any& value) const {
    // Color to float conversion not implemented
    return 0.0f;
}

// =============================================================================
// LambdaConverter
// =============================================================================

LambdaConverter::LambdaConverter(ConvertFunc convert, ConvertFunc convert_back)
    : m_convert(std::move(convert)), m_convert_back(std::move(convert_back)) {}

std::any LambdaConverter::convert(const std::any& value) const {
    if (m_convert) {
        return m_convert(value);
    }
    return value;
}

std::any LambdaConverter::convert_back(const std::any& value) const {
    if (m_convert_back) {
        return m_convert_back(value);
    }
    return value;
}

// =============================================================================
// PropertyBinding
// =============================================================================

PropertyBinding::PropertyBinding() = default;
PropertyBinding::~PropertyBinding() {
    deactivate();
}

void PropertyBinding::set_source(IDataSource* source, const std::string& path) {
    m_source = source;
    m_source_path = path;
}

void PropertyBinding::set_target(IHudElement* element, const std::string& property) {
    m_target = element;
    m_target_property = property;
}

void PropertyBinding::activate() {
    if (m_active || !m_source || !m_target) return;

    m_source->subscribe(m_source_path, [this](const std::any& value) {
        on_source_changed(value);
    });

    m_active = true;

    // Initial update
    update_target();
}

void PropertyBinding::deactivate() {
    if (!m_active || !m_source) return;

    m_source->unsubscribe(m_source_path);
    m_active = false;
}

void PropertyBinding::update_target() {
    if (!m_source || !m_target) return;

    auto value = m_source->get_property(m_source_path);
    apply_to_target(value);
}

void PropertyBinding::update_source() {
    if (!m_source || !m_target) return;
    if (m_mode != BindingMode::TwoWay && m_mode != BindingMode::OneWayToSource) return;

    auto value = get_target_value();
    if (m_converter) {
        value = m_converter->convert_back(value);
    }

    m_updating = true;
    m_source->set_property(m_source_path, value);
    m_updating = false;
}

void PropertyBinding::on_source_changed(const std::any& value) {
    if (m_updating) return;
    if (m_mode == BindingMode::OneWayToSource) return;

    apply_to_target(value);
}

void PropertyBinding::apply_to_target(const std::any& value) {
    if (!m_target) return;

    std::any converted = value;
    if (m_converter) {
        converted = m_converter->convert(value);
    }

    // Apply to specific property
    if (m_target_property == "text") {
        if (auto* text = dynamic_cast<HudText*>(m_target)) {
            try {
                text->set_text(std::any_cast<std::string>(converted));
            } catch (...) {}
        }
    } else if (m_target_property == "value") {
        if (auto* bar = dynamic_cast<HudProgressBar*>(m_target)) {
            try {
                bar->set_value(std::any_cast<float>(converted));
            } catch (...) {}
        }
    } else if (m_target_property == "visible") {
        try {
            m_target->set_visible(std::any_cast<bool>(converted));
        } catch (...) {}
    } else if (m_target_property == "opacity") {
        try {
            m_target->set_opacity(std::any_cast<float>(converted));
        } catch (...) {}
    }
    // Add more properties as needed
}

std::any PropertyBinding::get_target_value() const {
    if (!m_target) return std::any{};

    if (m_target_property == "text") {
        if (auto* text = dynamic_cast<HudText*>(m_target)) {
            return text->text();
        }
    } else if (m_target_property == "value") {
        if (auto* bar = dynamic_cast<HudProgressBar*>(m_target)) {
            return bar->value();
        }
    } else if (m_target_property == "visible") {
        return m_target->is_visible();
    } else if (m_target_property == "opacity") {
        return m_target->opacity();
    }

    return std::any{};
}

// =============================================================================
// BindingContext
// =============================================================================

BindingContext::BindingContext() = default;

BindingContext::BindingContext(IDataSource* data_source)
    : m_data_source(data_source) {}

BindingContext::~BindingContext() {
    clear_bindings();
}

PropertyBinding* BindingContext::create_binding() {
    auto binding = std::make_unique<PropertyBinding>();
    binding->set_id(BindingId{m_next_id++});
    PropertyBinding* ptr = binding.get();
    m_bindings.push_back(std::move(binding));
    return ptr;
}

PropertyBinding* BindingContext::bind(IHudElement* element, const std::string& property,
                                       const std::string& source_path, BindingMode mode) {
    return bind(element, property, source_path, nullptr, mode);
}

PropertyBinding* BindingContext::bind(IHudElement* element, const std::string& property,
                                       const std::string& source_path,
                                       std::shared_ptr<IValueConverter> converter,
                                       BindingMode mode) {
    if (!m_data_source || !element) return nullptr;

    auto* binding = create_binding();
    binding->set_source(m_data_source, source_path);
    binding->set_target(element, property);
    binding->set_mode(mode);
    binding->set_converter(converter);
    binding->activate();

    return binding;
}

void BindingContext::remove_binding(BindingId id) {
    m_bindings.erase(
        std::remove_if(m_bindings.begin(), m_bindings.end(),
                       [id](const auto& b) { return b->id() == id; }),
        m_bindings.end());
}

void BindingContext::remove_bindings_for_element(IHudElement* element) {
    for (auto& binding : m_bindings) {
        // Would need to track target element
    }
}

void BindingContext::clear_bindings() {
    for (auto& binding : m_bindings) {
        binding->deactivate();
    }
    m_bindings.clear();
}

std::vector<PropertyBinding*> BindingContext::get_bindings_for_element(IHudElement* element) {
    std::vector<PropertyBinding*> result;
    // Would need to track target element
    return result;
}

PropertyBinding* BindingContext::get_binding(BindingId id) {
    for (auto& binding : m_bindings) {
        if (binding->id() == id) {
            return binding.get();
        }
    }
    return nullptr;
}

void BindingContext::activate_all() {
    for (auto& binding : m_bindings) {
        binding->activate();
    }
}

void BindingContext::deactivate_all() {
    for (auto& binding : m_bindings) {
        binding->deactivate();
    }
}

void BindingContext::update_all() {
    for (auto& binding : m_bindings) {
        if (binding->is_active()) {
            binding->update_target();
        }
    }
}

// =============================================================================
// DataBindingManager
// =============================================================================

DataBindingManager::DataBindingManager() = default;
DataBindingManager::~DataBindingManager() = default;

void DataBindingManager::register_source(const std::string& name, IDataSource* source) {
    m_sources[name] = source;
}

void DataBindingManager::unregister_source(const std::string& name) {
    m_sources.erase(name);
}

IDataSource* DataBindingManager::get_source(const std::string& name) {
    auto it = m_sources.find(name);
    return it != m_sources.end() ? it->second : nullptr;
}

BindingContext* DataBindingManager::create_context(const std::string& name) {
    auto context = std::make_unique<BindingContext>();
    BindingContext* ptr = context.get();
    m_contexts[name] = std::move(context);
    return ptr;
}

void DataBindingManager::remove_context(const std::string& name) {
    m_contexts.erase(name);
}

BindingContext* DataBindingManager::get_context(const std::string& name) {
    auto it = m_contexts.find(name);
    return it != m_contexts.end() ? it->second.get() : nullptr;
}

BindingContext* DataBindingManager::get_or_create_context(const std::string& name) {
    auto* ctx = get_context(name);
    return ctx ? ctx : create_context(name);
}

PropertyBinding* DataBindingManager::bind(const std::string& source_name,
                                           const std::string& source_path,
                                           IHudElement* element,
                                           const std::string& property,
                                           BindingMode mode) {
    auto* source = get_source(source_name);
    if (!source) return nullptr;

    m_global_context.set_data_source(source);
    return m_global_context.bind(element, property, source_path, mode);
}

void DataBindingManager::bind_text(HudText* text, const std::string& source_name,
                                    const std::string& path, const std::string& format) {
    auto converter = std::make_shared<StringFormatConverter>(format);
    auto* source = get_source(source_name);
    if (!source || !text) return;

    m_global_context.set_data_source(source);
    m_global_context.bind(text, "text", path, converter);
}

void DataBindingManager::bind_progress(HudProgressBar* bar, const std::string& source_name,
                                        const std::string& value_path, const std::string& max_path) {
    auto* source = get_source(source_name);
    if (!source || !bar) return;

    m_global_context.set_data_source(source);
    m_global_context.bind(bar, "value", value_path);
    // max_path binding would need additional implementation
}

void DataBindingManager::bind_visibility(IHudElement* element, const std::string& source_name,
                                          const std::string& path, bool invert) {
    auto* source = get_source(source_name);
    if (!source || !element) return;

    std::shared_ptr<IValueConverter> converter;
    if (invert) {
        converter = std::make_shared<LambdaConverter>([](const std::any& v) {
            try {
                return !std::any_cast<bool>(v);
            } catch (...) {
                return false;
            }
        });
    }

    m_global_context.set_data_source(source);
    m_global_context.bind(element, "visible", path, converter);
}

void DataBindingManager::bind_color(IHudElement* element, const std::string& source_name,
                                     const std::string& path) {
    auto* source = get_source(source_name);
    if (!source || !element) return;

    m_global_context.set_data_source(source);
    // Color binding would need additional implementation
}

PropertyBinding* DataBindingManager::bind_expression(IHudElement* element,
                                                      const std::string& property,
                                                      const std::string& expression) {
    // Expression evaluation not implemented
    return nullptr;
}

void DataBindingManager::update() {
    m_global_context.update_all();
    for (auto& [_, context] : m_contexts) {
        context->update_all();
    }
}

void DataBindingManager::clear() {
    m_global_context.clear_bindings();
    m_contexts.clear();
    m_sources.clear();
}

// =============================================================================
// BindingBuilder
// =============================================================================

BindingBuilder::BindingBuilder(DataBindingManager* manager, IHudElement* element)
    : m_manager(manager), m_element(element) {}

BindingBuilder& BindingBuilder::to_property(const std::string& property) {
    m_property = property;
    return *this;
}

BindingBuilder& BindingBuilder::from_source(const std::string& source_name) {
    m_source_name = source_name;
    return *this;
}

BindingBuilder& BindingBuilder::from_path(const std::string& path) {
    m_source_path = path;
    return *this;
}

BindingBuilder& BindingBuilder::with_mode(BindingMode mode) {
    m_mode = mode;
    return *this;
}

BindingBuilder& BindingBuilder::with_format(const std::string& format) {
    m_converter = std::make_shared<StringFormatConverter>(format);
    return *this;
}

BindingBuilder& BindingBuilder::with_converter(std::shared_ptr<IValueConverter> converter) {
    m_converter = std::move(converter);
    return *this;
}

BindingBuilder& BindingBuilder::clamped(float min_val, float max_val) {
    m_converter = std::make_shared<ClampConverter>(min_val, max_val);
    return *this;
}

BindingBuilder& BindingBuilder::normalized(float min_val, float max_val) {
    m_converter = std::make_shared<NormalizeConverter>(min_val, max_val);
    return *this;
}

BindingBuilder& BindingBuilder::two_way() {
    m_mode = BindingMode::TwoWay;
    return *this;
}

PropertyBinding* BindingBuilder::build() {
    if (!m_manager || !m_element || m_property.empty() || m_source_path.empty()) {
        return nullptr;
    }

    auto* source = m_manager->get_source(m_source_name);
    if (!source) return nullptr;

    auto* context = m_manager->get_or_create_context("_builder");
    context->set_data_source(source);

    return context->bind(m_element, m_property, m_source_path, m_converter, m_mode);
}

} // namespace void_hud
