/// @file binding.hpp
/// @brief Data binding system for void_hud module

#pragma once

#include "fwd.hpp"
#include "types.hpp"

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace void_hud {

// =============================================================================
// IDataSource Interface
// =============================================================================

/// @brief Interface for data sources that can be bound to HUD elements
class IDataSource {
public:
    virtual ~IDataSource() = default;

    /// @brief Get property value by path
    virtual std::any get_property(std::string_view path) const = 0;

    /// @brief Set property value by path
    virtual bool set_property(std::string_view path, const std::any& value) = 0;

    /// @brief Check if property exists
    virtual bool has_property(std::string_view path) const = 0;

    /// @brief Subscribe to property changes
    virtual void subscribe(std::string_view path, std::function<void(const std::any&)> callback) = 0;

    /// @brief Unsubscribe from property changes
    virtual void unsubscribe(std::string_view path) = 0;

    /// @brief Notify that a property has changed
    virtual void notify_property_changed(std::string_view path) = 0;
};

// =============================================================================
// SimpleDataSource
// =============================================================================

/// @brief Simple map-based data source
class SimpleDataSource : public IDataSource {
public:
    SimpleDataSource() = default;
    ~SimpleDataSource() override = default;

    // Property access
    std::any get_property(std::string_view path) const override;
    bool set_property(std::string_view path, const std::any& value) override;
    bool has_property(std::string_view path) const override;

    // Subscriptions
    void subscribe(std::string_view path, std::function<void(const std::any&)> callback) override;
    void unsubscribe(std::string_view path) override;
    void notify_property_changed(std::string_view path) override;

    // Typed setters
    template<typename T>
    void set(const std::string& path, const T& value) {
        m_properties[path] = value;
        notify_property_changed(path);
    }

    template<typename T>
    T get(const std::string& path, const T& default_value = T{}) const {
        auto it = m_properties.find(path);
        if (it == m_properties.end()) return default_value;
        try {
            return std::any_cast<T>(it->second);
        } catch (...) {
            return default_value;
        }
    }

    // Clear
    void clear() { m_properties.clear(); }

private:
    std::unordered_map<std::string, std::any> m_properties;
    std::unordered_map<std::string, std::vector<std::function<void(const std::any&)>>> m_subscribers;
};

// =============================================================================
// ValueConverter
// =============================================================================

/// @brief Converts values between source and target types
class IValueConverter {
public:
    virtual ~IValueConverter() = default;

    /// @brief Convert from source to target
    virtual std::any convert(const std::any& value) const = 0;

    /// @brief Convert from target back to source
    virtual std::any convert_back(const std::any& value) const = 0;
};

/// @brief String format converter
class StringFormatConverter : public IValueConverter {
public:
    explicit StringFormatConverter(const std::string& format = "{}");

    std::any convert(const std::any& value) const override;
    std::any convert_back(const std::any& value) const override;

    void set_format(const std::string& format) { m_format = format; }

private:
    std::string m_format;
};

/// @brief Numeric clamping converter
class ClampConverter : public IValueConverter {
public:
    ClampConverter(float min_val, float max_val);

    std::any convert(const std::any& value) const override;
    std::any convert_back(const std::any& value) const override;

private:
    float m_min;
    float m_max;
};

/// @brief Normalizing converter (maps range to 0-1)
class NormalizeConverter : public IValueConverter {
public:
    NormalizeConverter(float min_val, float max_val);

    std::any convert(const std::any& value) const override;
    std::any convert_back(const std::any& value) const override;

private:
    float m_min;
    float m_max;
};

/// @brief Color interpolation converter
class ColorInterpolateConverter : public IValueConverter {
public:
    ColorInterpolateConverter(const Color& from, const Color& to);

    std::any convert(const std::any& value) const override;
    std::any convert_back(const std::any& value) const override;

private:
    Color m_from;
    Color m_to;
};

/// @brief Lambda-based custom converter
class LambdaConverter : public IValueConverter {
public:
    using ConvertFunc = std::function<std::any(const std::any&)>;

    LambdaConverter(ConvertFunc convert, ConvertFunc convert_back = nullptr);

    std::any convert(const std::any& value) const override;
    std::any convert_back(const std::any& value) const override;

private:
    ConvertFunc m_convert;
    ConvertFunc m_convert_back;
};

// =============================================================================
// PropertyBinding
// =============================================================================

/// @brief Binding between a data source property and a HUD element property
class PropertyBinding {
public:
    PropertyBinding();
    ~PropertyBinding();

    // Configuration
    void set_source(IDataSource* source, const std::string& path);
    void set_target(IHudElement* element, const std::string& property);
    void set_mode(BindingMode mode) { m_mode = mode; }
    void set_converter(std::shared_ptr<IValueConverter> converter) { m_converter = converter; }

    // State
    BindingId id() const { return m_id; }
    bool is_active() const { return m_active; }

    // Activate/Deactivate
    void activate();
    void deactivate();

    // Manual update
    void update_target();
    void update_source();

    // Internal
    void set_id(BindingId id) { m_id = id; }

private:
    void on_source_changed(const std::any& value);
    void apply_to_target(const std::any& value);
    std::any get_target_value() const;

    BindingId m_id;
    IDataSource* m_source{nullptr};
    std::string m_source_path;
    IHudElement* m_target{nullptr};
    std::string m_target_property;
    BindingMode m_mode{BindingMode::OneWay};
    std::shared_ptr<IValueConverter> m_converter;
    bool m_active{false};
    bool m_updating{false}; // Prevent infinite loops
};

// =============================================================================
// BindingContext
// =============================================================================

/// @brief Context for managing bindings within a scope
class BindingContext {
public:
    BindingContext();
    explicit BindingContext(IDataSource* data_source);
    ~BindingContext();

    // Data source
    void set_data_source(IDataSource* source) { m_data_source = source; }
    IDataSource* data_source() { return m_data_source; }
    const IDataSource* data_source() const { return m_data_source; }

    // Create bindings
    PropertyBinding* create_binding();
    PropertyBinding* bind(IHudElement* element, const std::string& property,
                          const std::string& source_path,
                          BindingMode mode = BindingMode::OneWay);

    // With converter
    PropertyBinding* bind(IHudElement* element, const std::string& property,
                          const std::string& source_path,
                          std::shared_ptr<IValueConverter> converter,
                          BindingMode mode = BindingMode::OneWay);

    // Remove bindings
    void remove_binding(BindingId id);
    void remove_bindings_for_element(IHudElement* element);
    void clear_bindings();

    // Query
    std::vector<PropertyBinding*> get_bindings_for_element(IHudElement* element);
    PropertyBinding* get_binding(BindingId id);

    // Activation
    void activate_all();
    void deactivate_all();

    // Update
    void update_all();

private:
    IDataSource* m_data_source{nullptr};
    std::vector<std::unique_ptr<PropertyBinding>> m_bindings;
    std::uint64_t m_next_id{1};
};

// =============================================================================
// DataBindingManager
// =============================================================================

/// @brief Global manager for data bindings
class DataBindingManager {
public:
    DataBindingManager();
    ~DataBindingManager();

    // Data sources
    void register_source(const std::string& name, IDataSource* source);
    void unregister_source(const std::string& name);
    IDataSource* get_source(const std::string& name);

    // Binding contexts
    BindingContext* create_context(const std::string& name = "");
    void remove_context(const std::string& name);
    BindingContext* get_context(const std::string& name);
    BindingContext* get_or_create_context(const std::string& name);

    // Global bindings
    PropertyBinding* bind(const std::string& source_name,
                          const std::string& source_path,
                          IHudElement* element,
                          const std::string& property,
                          BindingMode mode = BindingMode::OneWay);

    // Convenience for common patterns
    void bind_text(HudText* text, const std::string& source_name, const std::string& path,
                   const std::string& format = "{}");
    void bind_progress(HudProgressBar* bar, const std::string& source_name,
                       const std::string& value_path, const std::string& max_path = "");
    void bind_visibility(IHudElement* element, const std::string& source_name,
                         const std::string& path, bool invert = false);
    void bind_color(IHudElement* element, const std::string& source_name,
                    const std::string& path);

    // Expressions (bind to calculated values)
    PropertyBinding* bind_expression(IHudElement* element,
                                     const std::string& property,
                                     const std::string& expression);

    // Update
    void update();

    // Clear
    void clear();

private:
    std::unordered_map<std::string, IDataSource*> m_sources;
    std::unordered_map<std::string, std::unique_ptr<BindingContext>> m_contexts;
    BindingContext m_global_context;
};

// =============================================================================
// BindingBuilder
// =============================================================================

/// @brief Fluent builder for creating bindings
class BindingBuilder {
public:
    BindingBuilder(DataBindingManager* manager, IHudElement* element);

    BindingBuilder& to_property(const std::string& property);
    BindingBuilder& from_source(const std::string& source_name);
    BindingBuilder& from_path(const std::string& path);
    BindingBuilder& with_mode(BindingMode mode);
    BindingBuilder& with_format(const std::string& format);
    BindingBuilder& with_converter(std::shared_ptr<IValueConverter> converter);
    BindingBuilder& clamped(float min_val, float max_val);
    BindingBuilder& normalized(float min_val, float max_val);
    BindingBuilder& two_way();

    PropertyBinding* build();

private:
    DataBindingManager* m_manager;
    IHudElement* m_element;
    std::string m_property;
    std::string m_source_name;
    std::string m_source_path;
    BindingMode m_mode{BindingMode::OneWay};
    std::shared_ptr<IValueConverter> m_converter;
};

// =============================================================================
// Observable Pattern Helpers
// =============================================================================

/// @brief Observable value that notifies on change
template<typename T>
class Observable {
public:
    Observable() = default;
    explicit Observable(const T& value) : m_value(value) {}

    const T& get() const { return m_value; }

    void set(const T& value) {
        if (m_value != value) {
            T old_value = m_value;
            m_value = value;
            for (auto& callback : m_callbacks) {
                callback(old_value, m_value);
            }
        }
    }

    Observable& operator=(const T& value) {
        set(value);
        return *this;
    }

    operator const T&() const { return m_value; }

    void subscribe(std::function<void(const T&, const T&)> callback) {
        m_callbacks.push_back(std::move(callback));
    }

    void clear_subscriptions() {
        m_callbacks.clear();
    }

private:
    T m_value{};
    std::vector<std::function<void(const T&, const T&)>> m_callbacks;
};

} // namespace void_hud
