/// @file scene_parser.cpp
/// @brief TOML and JSON scene parser implementation
/// @details Full TOML parser matching legacy Rust scene_loader.rs

#include "scene_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace void_runtime {

// =============================================================================
// TomlValue Implementation
// =============================================================================

const TomlValue TomlValue::null_value_;
const TomlArray TomlValue::empty_array_;
const TomlTable TomlValue::empty_table_;

bool TomlValue::as_bool(bool def) const {
    if (is_bool()) return std::get<bool>(value_);
    if (is_int()) return std::get<std::int64_t>(value_) != 0;
    if (is_string()) {
        auto s = std::get<std::string>(value_);
        return s == "true" || s == "1" || s == "yes";
    }
    return def;
}

std::int64_t TomlValue::as_int(std::int64_t def) const {
    if (is_int()) return std::get<std::int64_t>(value_);
    if (is_float()) return static_cast<std::int64_t>(std::get<double>(value_));
    if (is_bool()) return std::get<bool>(value_) ? 1 : 0;
    if (is_string()) {
        try { return std::stoll(std::get<std::string>(value_)); }
        catch (...) { return def; }
    }
    return def;
}

double TomlValue::as_float(double def) const {
    if (is_float()) return std::get<double>(value_);
    if (is_int()) return static_cast<double>(std::get<std::int64_t>(value_));
    if (is_string()) {
        try { return std::stod(std::get<std::string>(value_)); }
        catch (...) { return def; }
    }
    return def;
}

std::string TomlValue::as_string(const std::string& def) const {
    if (is_string()) return std::get<std::string>(value_);
    if (is_int()) return std::to_string(std::get<std::int64_t>(value_));
    if (is_float()) return std::to_string(std::get<double>(value_));
    if (is_bool()) return std::get<bool>(value_) ? "true" : "false";
    return def;
}

const TomlArray& TomlValue::as_array() const {
    if (is_array()) return std::get<TomlArray>(value_);
    return empty_array_;
}

const TomlTable& TomlValue::as_table() const {
    if (is_table()) return *std::get<std::shared_ptr<TomlTable>>(value_);
    return empty_table_;
}

TomlTable& TomlValue::as_table_mut() {
    if (!is_table()) {
        value_ = std::make_shared<TomlTable>();
    }
    return *std::get<std::shared_ptr<TomlTable>>(value_);
}

const TomlValue& TomlValue::operator[](const std::string& key) const {
    if (is_table()) {
        const auto& table = as_table();
        auto it = table.find(key);
        if (it != table.end()) return it->second;
    }
    return null_value_;
}

const TomlValue& TomlValue::operator[](std::size_t index) const {
    if (is_array()) {
        const auto& arr = as_array();
        if (index < arr.size()) return arr[index];
    }
    return null_value_;
}

bool TomlValue::has(const std::string& key) const {
    if (is_table()) {
        return as_table().count(key) > 0;
    }
    return false;
}

std::size_t TomlValue::size() const {
    if (is_array()) return as_array().size();
    if (is_table()) return as_table().size();
    return 0;
}

Vec2 TomlValue::as_vec2(const Vec2& def) const {
    if (is_array() && as_array().size() >= 2) {
        return {
            static_cast<float>(as_array()[0].as_float(def[0])),
            static_cast<float>(as_array()[1].as_float(def[1]))
        };
    }
    return def;
}

Vec3 TomlValue::as_vec3(const Vec3& def) const {
    if (is_array() && as_array().size() >= 3) {
        return {
            static_cast<float>(as_array()[0].as_float(def[0])),
            static_cast<float>(as_array()[1].as_float(def[1])),
            static_cast<float>(as_array()[2].as_float(def[2]))
        };
    }
    return def;
}

Vec4 TomlValue::as_vec4(const Vec4& def) const {
    if (is_array() && as_array().size() >= 4) {
        return {
            static_cast<float>(as_array()[0].as_float(def[0])),
            static_cast<float>(as_array()[1].as_float(def[1])),
            static_cast<float>(as_array()[2].as_float(def[2])),
            static_cast<float>(as_array()[3].as_float(def[3]))
        };
    }
    return def;
}

Color3 TomlValue::as_color3(const Color3& def) const {
    return as_vec3(def);
}

Color4 TomlValue::as_color4(const Color4& def) const {
    return as_vec4(def);
}

// =============================================================================
// TOML Parser Implementation
// =============================================================================

thread_local std::string TomlParser::last_error_;

std::string TomlParser::last_error() {
    return last_error_;
}

std::optional<TomlValue> TomlParser::parse(const std::string& content) {
    ParserState state;
    state.input = content.c_str();
    state.current = content.c_str();
    state.end = content.c_str() + content.size();

    TomlTable root;
    if (!parse_document(state, root)) {
        last_error_ = state.error;
        return std::nullopt;
    }

    return TomlValue(std::move(root));
}

std::optional<TomlValue> TomlParser::parse_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        last_error_ = "Failed to open file: " + path.string();
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return parse(ss.str());
}

bool TomlParser::parse_document(ParserState& state, TomlTable& root) {
    TomlTable* current_table = &root;
    std::vector<std::string> current_path;

    while (!at_end(state)) {
        skip_whitespace_and_newlines(state);
        if (at_end(state)) break;

        // Skip comments
        if (current(state) == '#') {
            skip_to_newline(state);
            continue;
        }

        // Table header
        if (current(state) == '[') {
            std::vector<std::string> path;
            bool is_array = false;
            if (!parse_table_header(state, path, is_array)) {
                return false;
            }

            if (is_array) {
                auto* arr = get_or_create_array(root, path);
                if (!arr) {
                    set_error(state, "Failed to create table array");
                    return false;
                }
                arr->push_back(TomlValue(TomlTable{}));
                current_table = &arr->back().as_table_mut();
            } else {
                current_table = get_or_create_table(root, path);
                if (!current_table) {
                    set_error(state, "Failed to create table");
                    return false;
                }
            }
            current_path = path;
            continue;
        }

        // Key-value pair
        if (!parse_key_value(state, *current_table)) {
            return false;
        }
    }

    return true;
}

bool TomlParser::parse_table_header(ParserState& state, std::vector<std::string>& path, bool& is_array) {
    is_array = false;

    if (!expect(state, '[')) return false;

    // Check for array of tables
    if (peek(state, '[')) {
        advance(state);
        is_array = true;
    }

    skip_whitespace(state);

    // Parse path
    while (true) {
        std::string key;
        if (!parse_key(state, key)) {
            set_error(state, "Expected key in table header");
            return false;
        }
        path.push_back(key);

        skip_whitespace(state);

        if (current(state) == '.') {
            advance(state);
            skip_whitespace(state);
        } else {
            break;
        }
    }

    skip_whitespace(state);

    if (is_array) {
        if (!expect(state, ']')) return false;
    }
    if (!expect(state, ']')) return false;

    skip_to_newline(state);
    return true;
}

bool TomlParser::parse_key_value(ParserState& state, TomlTable& table) {
    std::string key;
    if (!parse_key(state, key)) {
        set_error(state, "Expected key");
        return false;
    }

    skip_whitespace(state);

    if (!expect(state, '=')) {
        set_error(state, "Expected '=' after key");
        return false;
    }

    skip_whitespace(state);

    TomlValue value;
    if (!parse_value(state, value)) {
        return false;
    }

    // Handle dotted keys
    std::size_t dot_pos = key.find('.');
    if (dot_pos != std::string::npos) {
        std::vector<std::string> parts;
        std::istringstream iss(key);
        std::string part;
        while (std::getline(iss, part, '.')) {
            parts.push_back(part);
        }

        TomlTable* current = &table;
        for (std::size_t i = 0; i < parts.size() - 1; ++i) {
            auto it = current->find(parts[i]);
            if (it == current->end()) {
                (*current)[parts[i]] = TomlValue(TomlTable{});
                it = current->find(parts[i]);
            }
            if (!it->second.is_table()) {
                set_error(state, "Key is not a table: " + parts[i]);
                return false;
            }
            current = &it->second.as_table_mut();
        }
        (*current)[parts.back()] = std::move(value);
    } else {
        table[key] = std::move(value);
    }

    skip_to_newline(state);
    return true;
}

bool TomlParser::parse_key(ParserState& state, std::string& key) {
    skip_whitespace(state);

    if (current(state) == '"') {
        return parse_basic_string(state, key);
    } else if (current(state) == '\'') {
        return parse_literal_string(state, key);
    } else if (is_bare_key_char(current(state))) {
        while (!at_end(state) && is_bare_key_char(current(state))) {
            key += current(state);
            advance(state);
        }
        return !key.empty();
    }

    return false;
}

bool TomlParser::parse_value(ParserState& state, TomlValue& value) {
    skip_whitespace(state);

    char c = current(state);

    // String
    if (c == '"' || c == '\'') {
        std::string str;
        if (!parse_string(state, str)) return false;
        value = TomlValue(str);
        return true;
    }

    // Array
    if (c == '[') {
        TomlArray arr;
        if (!parse_array(state, arr)) return false;
        value = TomlValue(std::move(arr));
        return true;
    }

    // Inline table
    if (c == '{') {
        TomlTable table;
        if (!parse_inline_table(state, table)) return false;
        value = TomlValue(std::move(table));
        return true;
    }

    // Boolean
    if (c == 't' || c == 'f') {
        bool b;
        if (parse_bool(state, b)) {
            value = TomlValue(b);
            return true;
        }
    }

    // Number (including dates which we'll treat as strings for now)
    if (c == '-' || c == '+' || std::isdigit(c)) {
        return parse_number(state, value);
    }

    set_error(state, std::string("Unexpected character: ") + c);
    return false;
}

bool TomlParser::parse_string(ParserState& state, std::string& str) {
    if (current(state) == '"') {
        advance(state);
        if (peek(state, '"') && state.current + 1 < state.end && *(state.current + 1) == '"') {
            advance(state);
            advance(state);
            return parse_multiline_basic_string(state, str);
        }
        // Put back the quote we consumed for checking
        state.current--;
        state.column--;
        return parse_basic_string(state, str);
    } else if (current(state) == '\'') {
        advance(state);
        if (peek(state, '\'') && state.current + 1 < state.end && *(state.current + 1) == '\'') {
            advance(state);
            advance(state);
            return parse_multiline_literal_string(state, str);
        }
        state.current--;
        state.column--;
        return parse_literal_string(state, str);
    }
    return false;
}

bool TomlParser::parse_basic_string(ParserState& state, std::string& str) {
    if (!expect(state, '"')) return false;

    while (!at_end(state) && current(state) != '"') {
        if (current(state) == '\\') {
            advance(state);
            if (at_end(state)) {
                set_error(state, "Unexpected end of string");
                return false;
            }
            switch (current(state)) {
                case 'b': str += '\b'; break;
                case 't': str += '\t'; break;
                case 'n': str += '\n'; break;
                case 'f': str += '\f'; break;
                case 'r': str += '\r'; break;
                case '"': str += '"'; break;
                case '\\': str += '\\'; break;
                case 'u': {
                    advance(state);
                    std::string hex;
                    for (int i = 0; i < 4 && !at_end(state); ++i) {
                        hex += current(state);
                        advance(state);
                    }
                    state.current--;
                    // Simple ASCII handling
                    try {
                        int code = std::stoi(hex, nullptr, 16);
                        if (code < 128) str += static_cast<char>(code);
                    } catch (...) {}
                    break;
                }
                default: str += current(state); break;
            }
        } else if (current(state) == '\n') {
            set_error(state, "Newline in basic string");
            return false;
        } else {
            str += current(state);
        }
        advance(state);
    }

    return expect(state, '"');
}

bool TomlParser::parse_literal_string(ParserState& state, std::string& str) {
    if (!expect(state, '\'')) return false;

    while (!at_end(state) && current(state) != '\'') {
        if (current(state) == '\n') {
            set_error(state, "Newline in literal string");
            return false;
        }
        str += current(state);
        advance(state);
    }

    return expect(state, '\'');
}

bool TomlParser::parse_multiline_basic_string(ParserState& state, std::string& str) {
    // Skip immediate newline
    if (current(state) == '\n') {
        advance(state);
    } else if (current(state) == '\r' && peek(state, '\n')) {
        advance(state);
        advance(state);
    }

    while (!at_end(state)) {
        if (current(state) == '"' && state.current + 2 < state.end &&
            *(state.current + 1) == '"' && *(state.current + 2) == '"') {
            advance(state);
            advance(state);
            advance(state);
            return true;
        }

        if (current(state) == '\\') {
            advance(state);
            if (at_end(state)) break;

            if (current(state) == '\n' || current(state) == '\r') {
                // Line ending backslash
                while (!at_end(state) && (current(state) == '\n' || current(state) == '\r' ||
                       current(state) == ' ' || current(state) == '\t')) {
                    advance(state);
                }
                continue;
            }

            switch (current(state)) {
                case 'b': str += '\b'; break;
                case 't': str += '\t'; break;
                case 'n': str += '\n'; break;
                case 'f': str += '\f'; break;
                case 'r': str += '\r'; break;
                case '"': str += '"'; break;
                case '\\': str += '\\'; break;
                default: str += current(state); break;
            }
        } else {
            str += current(state);
        }
        advance(state);
    }

    set_error(state, "Unterminated multiline string");
    return false;
}

bool TomlParser::parse_multiline_literal_string(ParserState& state, std::string& str) {
    // Skip immediate newline
    if (current(state) == '\n') {
        advance(state);
    } else if (current(state) == '\r' && peek(state, '\n')) {
        advance(state);
        advance(state);
    }

    while (!at_end(state)) {
        if (current(state) == '\'' && state.current + 2 < state.end &&
            *(state.current + 1) == '\'' && *(state.current + 2) == '\'') {
            advance(state);
            advance(state);
            advance(state);
            return true;
        }
        str += current(state);
        advance(state);
    }

    set_error(state, "Unterminated multiline literal string");
    return false;
}

bool TomlParser::parse_number(ParserState& state, TomlValue& value) {
    std::string num_str;
    bool is_float = false;
    bool is_negative = false;

    // Sign
    if (current(state) == '-') {
        is_negative = true;
        num_str += current(state);
        advance(state);
    } else if (current(state) == '+') {
        advance(state);
    }

    // Special values
    if (state.current + 2 < state.end) {
        if (std::strncmp(state.current, "inf", 3) == 0) {
            advance(state); advance(state); advance(state);
            value = TomlValue(is_negative ? -std::numeric_limits<double>::infinity()
                                          : std::numeric_limits<double>::infinity());
            return true;
        }
        if (std::strncmp(state.current, "nan", 3) == 0) {
            advance(state); advance(state); advance(state);
            value = TomlValue(std::numeric_limits<double>::quiet_NaN());
            return true;
        }
    }

    // Hex, octal, binary
    if (current(state) == '0' && state.current + 1 < state.end) {
        char next = *(state.current + 1);
        if (next == 'x' || next == 'X') {
            advance(state); advance(state);
            std::string hex;
            while (!at_end(state) && (std::isxdigit(current(state)) || current(state) == '_')) {
                if (current(state) != '_') hex += current(state);
                advance(state);
            }
            try {
                value = TomlValue(static_cast<std::int64_t>(std::stoll(hex, nullptr, 16)) *
                                  (is_negative ? -1 : 1));
                return true;
            } catch (...) {
                set_error(state, "Invalid hex number");
                return false;
            }
        }
        if (next == 'o' || next == 'O') {
            advance(state); advance(state);
            std::string oct;
            while (!at_end(state) && ((current(state) >= '0' && current(state) <= '7') ||
                   current(state) == '_')) {
                if (current(state) != '_') oct += current(state);
                advance(state);
            }
            try {
                value = TomlValue(static_cast<std::int64_t>(std::stoll(oct, nullptr, 8)) *
                                  (is_negative ? -1 : 1));
                return true;
            } catch (...) {
                set_error(state, "Invalid octal number");
                return false;
            }
        }
        if (next == 'b' || next == 'B') {
            advance(state); advance(state);
            std::string bin;
            while (!at_end(state) && (current(state) == '0' || current(state) == '1' ||
                   current(state) == '_')) {
                if (current(state) != '_') bin += current(state);
                advance(state);
            }
            try {
                value = TomlValue(static_cast<std::int64_t>(std::stoll(bin, nullptr, 2)) *
                                  (is_negative ? -1 : 1));
                return true;
            } catch (...) {
                set_error(state, "Invalid binary number");
                return false;
            }
        }
    }

    // Regular decimal or float
    while (!at_end(state) && (std::isdigit(current(state)) || current(state) == '_')) {
        if (current(state) != '_') num_str += current(state);
        advance(state);
    }

    // Check for date/time (YYYY-MM-DD)
    if (current(state) == '-' && num_str.length() == 4) {
        // It's a date, read as string
        num_str += current(state);
        advance(state);
        while (!at_end(state) && (std::isdigit(current(state)) || current(state) == '-' ||
               current(state) == 'T' || current(state) == ':' || current(state) == '.' ||
               current(state) == 'Z' || current(state) == '+')) {
            num_str += current(state);
            advance(state);
        }
        value = TomlValue(num_str);
        return true;
    }

    // Decimal point
    if (current(state) == '.') {
        is_float = true;
        num_str += current(state);
        advance(state);
        while (!at_end(state) && (std::isdigit(current(state)) || current(state) == '_')) {
            if (current(state) != '_') num_str += current(state);
            advance(state);
        }
    }

    // Exponent
    if (current(state) == 'e' || current(state) == 'E') {
        is_float = true;
        num_str += current(state);
        advance(state);
        if (current(state) == '+' || current(state) == '-') {
            num_str += current(state);
            advance(state);
        }
        while (!at_end(state) && (std::isdigit(current(state)) || current(state) == '_')) {
            if (current(state) != '_') num_str += current(state);
            advance(state);
        }
    }

    try {
        if (is_float) {
            value = TomlValue(std::stod(num_str));
        } else {
            value = TomlValue(std::stoll(num_str));
        }
        return true;
    } catch (...) {
        set_error(state, "Invalid number: " + num_str);
        return false;
    }
}

bool TomlParser::parse_bool(ParserState& state, bool& value) {
    if (state.current + 4 <= state.end && std::strncmp(state.current, "true", 4) == 0) {
        if (state.current + 4 >= state.end || !std::isalnum(*(state.current + 4))) {
            state.current += 4;
            state.column += 4;
            value = true;
            return true;
        }
    }
    if (state.current + 5 <= state.end && std::strncmp(state.current, "false", 5) == 0) {
        if (state.current + 5 >= state.end || !std::isalnum(*(state.current + 5))) {
            state.current += 5;
            state.column += 5;
            value = false;
            return true;
        }
    }
    return false;
}

bool TomlParser::parse_array(ParserState& state, TomlArray& arr) {
    if (!expect(state, '[')) return false;

    skip_whitespace_and_newlines(state);

    while (!at_end(state) && current(state) != ']') {
        // Skip comments
        while (current(state) == '#') {
            skip_to_newline(state);
            skip_whitespace_and_newlines(state);
        }

        if (current(state) == ']') break;

        TomlValue value;
        if (!parse_value(state, value)) {
            return false;
        }
        arr.push_back(std::move(value));

        skip_whitespace_and_newlines(state);

        // Skip comments
        while (current(state) == '#') {
            skip_to_newline(state);
            skip_whitespace_and_newlines(state);
        }

        if (current(state) == ',') {
            advance(state);
            skip_whitespace_and_newlines(state);
        }
    }

    return expect(state, ']');
}

bool TomlParser::parse_inline_table(ParserState& state, TomlTable& table) {
    if (!expect(state, '{')) return false;

    skip_whitespace(state);

    while (!at_end(state) && current(state) != '}') {
        std::string key;
        if (!parse_key(state, key)) {
            set_error(state, "Expected key in inline table");
            return false;
        }

        skip_whitespace(state);
        if (!expect(state, '=')) return false;
        skip_whitespace(state);

        TomlValue value;
        if (!parse_value(state, value)) {
            return false;
        }
        table[key] = std::move(value);

        skip_whitespace(state);

        if (current(state) == ',') {
            advance(state);
            skip_whitespace(state);
        }
    }

    return expect(state, '}');
}

void TomlParser::skip_whitespace(ParserState& state) {
    while (!at_end(state) && (current(state) == ' ' || current(state) == '\t')) {
        advance(state);
    }
}

void TomlParser::skip_whitespace_and_newlines(ParserState& state) {
    while (!at_end(state)) {
        char c = current(state);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance(state);
        } else if (c == '#') {
            skip_to_newline(state);
        } else {
            break;
        }
    }
}

void TomlParser::skip_comment(ParserState& state) {
    if (current(state) == '#') {
        skip_to_newline(state);
    }
}

void TomlParser::skip_to_newline(ParserState& state) {
    while (!at_end(state) && current(state) != '\n') {
        advance(state);
    }
    if (!at_end(state)) advance(state);
}

bool TomlParser::is_bare_key_char(char c) {
    return std::isalnum(c) || c == '_' || c == '-';
}

bool TomlParser::expect(ParserState& state, char c) {
    if (at_end(state) || current(state) != c) {
        set_error(state, std::string("Expected '") + c + "', got '" +
                  (at_end(state) ? "EOF" : std::string(1, current(state))) + "'");
        return false;
    }
    advance(state);
    return true;
}

bool TomlParser::peek(ParserState& state, char c) {
    return !at_end(state) && current(state) == c;
}

char TomlParser::current(ParserState& state) {
    return at_end(state) ? '\0' : *state.current;
}

void TomlParser::advance(ParserState& state) {
    if (!at_end(state)) {
        if (*state.current == '\n') {
            state.line++;
            state.column = 1;
        } else {
            state.column++;
        }
        state.current++;
    }
}

bool TomlParser::at_end(ParserState& state) {
    return state.current >= state.end;
}

void TomlParser::set_error(ParserState& state, const std::string& msg) {
    std::ostringstream ss;
    ss << "Line " << state.line << ", column " << state.column << ": " << msg;
    state.error = ss.str();
}

TomlTable* TomlParser::get_or_create_table(TomlTable& root, const std::vector<std::string>& path) {
    TomlTable* current = &root;
    for (const auto& key : path) {
        auto it = current->find(key);
        if (it == current->end()) {
            (*current)[key] = TomlValue(TomlTable{});
            it = current->find(key);
        }
        if (it->second.is_array()) {
            auto& arr = const_cast<TomlArray&>(it->second.as_array());
            if (arr.empty()) {
                arr.push_back(TomlValue(TomlTable{}));
            }
            current = &arr.back().as_table_mut();
        } else if (it->second.is_table()) {
            current = &it->second.as_table_mut();
        } else {
            return nullptr;
        }
    }
    return current;
}

TomlArray* TomlParser::get_or_create_array(TomlTable& root, const std::vector<std::string>& path) {
    if (path.empty()) return nullptr;

    TomlTable* parent = &root;
    for (std::size_t i = 0; i < path.size() - 1; ++i) {
        auto it = parent->find(path[i]);
        if (it == parent->end()) {
            (*parent)[path[i]] = TomlValue(TomlTable{});
            it = parent->find(path[i]);
        }
        if (it->second.is_array()) {
            auto& arr = const_cast<TomlArray&>(it->second.as_array());
            if (arr.empty()) return nullptr;
            parent = &arr.back().as_table_mut();
        } else if (it->second.is_table()) {
            parent = &it->second.as_table_mut();
        } else {
            return nullptr;
        }
    }

    const auto& last_key = path.back();
    auto it = parent->find(last_key);
    if (it == parent->end()) {
        (*parent)[last_key] = TomlValue(TomlArray{});
        it = parent->find(last_key);
    }
    if (!it->second.is_array()) {
        return nullptr;
    }
    return &const_cast<TomlArray&>(it->second.as_array());
}

// =============================================================================
// Scene Parser Implementation
// =============================================================================

thread_local std::string SceneParser::last_error_;

std::optional<SceneDefinition> SceneParser::parse_file(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        last_error_ = "Failed to open file: " + path.string();
        return std::nullopt;
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    if (extension == ".toml") {
        return parse_toml(content);
    } else if (extension == ".json") {
        return parse_json(content);
    } else {
        // Try TOML first, fall back to JSON
        auto result = parse_toml(content);
        if (!result) {
            result = parse_json(content);
        }
        return result;
    }
}

std::optional<SceneDefinition> SceneParser::parse_toml(const std::string& content) {
    auto toml = TomlParser::parse(content);
    if (!toml) {
        last_error_ = TomlParser::last_error();
        return std::nullopt;
    }

    SceneDefinition scene;

    // Parse scene metadata
    if (toml->has("scene")) {
        parse_scene_metadata((*toml)["scene"], scene.scene);
    }

    // Parse cameras
    if (toml->has("cameras")) {
        for (const auto& cam_value : (*toml)["cameras"].as_array()) {
            CameraDef camera;
            parse_camera(cam_value, camera);
            scene.cameras.push_back(std::move(camera));
        }
    }

    // Parse lights
    if (toml->has("lights")) {
        for (const auto& light_value : (*toml)["lights"].as_array()) {
            LightDef light;
            parse_light(light_value, light);
            scene.lights.push_back(std::move(light));
        }
    }

    // Parse shadows
    if (toml->has("shadows")) {
        parse_shadows((*toml)["shadows"], scene.shadows);
    }

    // Parse environment
    if (toml->has("environment")) {
        parse_environment((*toml)["environment"], scene.environment);
    }

    // Parse picking
    if (toml->has("picking")) {
        parse_picking((*toml)["picking"], scene.picking);
    }

    // Parse spatial
    if (toml->has("spatial")) {
        parse_spatial((*toml)["spatial"], scene.spatial);
    }

    // Parse debug
    if (toml->has("debug")) {
        parse_debug((*toml)["debug"], scene.debug);
    }

    // Parse input
    if (toml->has("input")) {
        parse_input((*toml)["input"], scene.input);
    }

    // Parse entities
    if (toml->has("entities")) {
        for (const auto& entity_value : (*toml)["entities"].as_array()) {
            EntityDef entity;
            parse_entity(entity_value, entity);
            scene.entities.push_back(std::move(entity));
        }
    }

    // Parse particle emitters
    if (toml->has("particle_emitters")) {
        for (const auto& emitter_value : (*toml)["particle_emitters"].as_array()) {
            ParticleEmitterDef emitter;
            parse_particle_emitter(emitter_value, emitter);
            scene.particle_emitters.push_back(std::move(emitter));
        }
    }

    // Parse textures
    if (toml->has("textures")) {
        for (const auto& tex_value : (*toml)["textures"].as_array()) {
            TextureDef texture;
            parse_texture(tex_value, texture);
            scene.textures.push_back(std::move(texture));
        }
    }

    // Parse items
    if (toml->has("items")) {
        for (const auto& item_value : (*toml)["items"].as_array()) {
            ItemDef item;
            parse_item(item_value, item);
            scene.items.push_back(std::move(item));
        }
    }

    // Parse status effects
    if (toml->has("status_effects")) {
        for (const auto& effect_value : (*toml)["status_effects"].as_array()) {
            StatusEffectDef effect;
            parse_status_effect(effect_value, effect);
            scene.status_effects.push_back(std::move(effect));
        }
    }

    // Parse quests
    if (toml->has("quests")) {
        for (const auto& quest_value : (*toml)["quests"].as_array()) {
            QuestDef quest;
            parse_quest(quest_value, quest);
            scene.quests.push_back(std::move(quest));
        }
    }

    // Parse loot tables
    if (toml->has("loot_tables")) {
        for (const auto& loot_value : (*toml)["loot_tables"].as_array()) {
            LootTableDef loot;
            parse_loot_table(loot_value, loot);
            scene.loot_tables.push_back(std::move(loot));
        }
    }

    // Parse audio
    if (toml->has("audio")) {
        AudioConfigDef audio;
        parse_audio_config((*toml)["audio"], audio);
        scene.audio = std::move(audio);
    }

    // Parse navigation
    if (toml->has("navigation")) {
        NavigationConfigDef nav;
        parse_navigation_config((*toml)["navigation"], nav);
        scene.navigation = std::move(nav);
    }

    // Parse prefabs list
    if (toml->has("prefabs")) {
        for (const auto& prefab : (*toml)["prefabs"].as_array()) {
            scene.prefabs.push_back(prefab.as_string());
        }
    }

    return scene;
}

std::optional<SceneDefinition> SceneParser::parse_json(const std::string& content) {
    // Simple JSON to TOML-like structure converter
    // For full JSON support, would use a proper JSON library
    // This provides basic compatibility
    last_error_ = "JSON parsing not yet implemented - use TOML format";
    return std::nullopt;
}

// =============================================================================
// Parse Helpers Implementation
// =============================================================================

void SceneParser::parse_scene_metadata(const TomlValue& value, SceneMetadata& metadata) {
    metadata.name = value["name"].as_string(metadata.name);
    metadata.description = value["description"].as_string();
    metadata.version = value["version"].as_string(metadata.version);
    metadata.author = value["author"].as_string();

    if (value.has("tags")) {
        for (const auto& tag : value["tags"].as_array()) {
            metadata.tags.push_back(tag.as_string());
        }
    }
}

void SceneParser::parse_camera(const TomlValue& value, CameraDef& camera) {
    camera.name = value["name"].as_string();
    camera.active = value["active"].as_bool(false);
    camera.type = parse_camera_type(value["type"].as_string("perspective"));
    camera.control_mode = parse_camera_control_mode(value["control_mode"].as_string("none"));

    // Transform
    if (value.has("transform")) {
        const auto& t = value["transform"];
        camera.transform.position = t["position"].as_vec3(camera.transform.position);
        camera.transform.target = t["target"].as_vec3(camera.transform.target);
        camera.transform.up = t["up"].as_vec3(camera.transform.up);
    }

    // Perspective
    if (value.has("perspective")) {
        const auto& p = value["perspective"];
        camera.perspective.fov = static_cast<float>(p["fov"].as_float(camera.perspective.fov));
        camera.perspective.near_plane = static_cast<float>(p["near"].as_float(camera.perspective.near_plane));
        camera.perspective.far_plane = static_cast<float>(p["far"].as_float(camera.perspective.far_plane));
        camera.perspective.aspect = p["aspect"].as_string(camera.perspective.aspect);
    }

    // Orthographic
    if (value.has("orthographic")) {
        const auto& o = value["orthographic"];
        camera.orthographic.left = static_cast<float>(o["left"].as_float(camera.orthographic.left));
        camera.orthographic.right = static_cast<float>(o["right"].as_float(camera.orthographic.right));
        camera.orthographic.bottom = static_cast<float>(o["bottom"].as_float(camera.orthographic.bottom));
        camera.orthographic.top = static_cast<float>(o["top"].as_float(camera.orthographic.top));
        camera.orthographic.near_plane = static_cast<float>(o["near"].as_float(camera.orthographic.near_plane));
        camera.orthographic.far_plane = static_cast<float>(o["far"].as_float(camera.orthographic.far_plane));
    }

    // Control settings
    camera.move_speed = static_cast<float>(value["move_speed"].as_float(camera.move_speed));
    camera.look_sensitivity = static_cast<float>(value["look_sensitivity"].as_float(camera.look_sensitivity));
    camera.zoom_speed = static_cast<float>(value["zoom_speed"].as_float(camera.zoom_speed));
    camera.invert_y = value["invert_y"].as_bool(camera.invert_y);

    // Follow settings
    camera.follow_target = value["follow_target"].as_string();
    camera.follow_offset = value["follow_offset"].as_vec3(camera.follow_offset);
    camera.follow_smoothing = static_cast<float>(value["follow_smoothing"].as_float(camera.follow_smoothing));
}

void SceneParser::parse_light(const TomlValue& value, LightDef& light) {
    light.name = value["name"].as_string();
    light.type = parse_light_type(value["type"].as_string("point"));
    light.enabled = value["enabled"].as_bool(true);
    light.layer = value["layer"].as_string("world");

    // Directional
    if (value.has("directional")) {
        const auto& d = value["directional"];
        light.directional.direction = d["direction"].as_vec3(light.directional.direction);
        light.directional.color = d["color"].as_color3(light.directional.color);
        light.directional.intensity = static_cast<float>(d["intensity"].as_float(light.directional.intensity));
        light.directional.cast_shadows = d["cast_shadows"].as_bool(light.directional.cast_shadows);
    }

    // Point
    if (value.has("point")) {
        const auto& p = value["point"];
        light.point.position = p["position"].as_vec3(light.point.position);
        light.point.color = p["color"].as_color3(light.point.color);
        light.point.intensity = static_cast<float>(p["intensity"].as_float(light.point.intensity));
        light.point.range = static_cast<float>(p["range"].as_float(light.point.range));
        light.point.cast_shadows = p["cast_shadows"].as_bool(light.point.cast_shadows);
    }

    // Spot
    if (value.has("spot")) {
        const auto& s = value["spot"];
        light.spot.position = s["position"].as_vec3(light.spot.position);
        light.spot.direction = s["direction"].as_vec3(light.spot.direction);
        light.spot.color = s["color"].as_color3(light.spot.color);
        light.spot.intensity = static_cast<float>(s["intensity"].as_float(light.spot.intensity));
        light.spot.range = static_cast<float>(s["range"].as_float(light.spot.range));
        light.spot.inner_angle = static_cast<float>(s["inner_angle"].as_float(light.spot.inner_angle));
        light.spot.outer_angle = static_cast<float>(s["outer_angle"].as_float(light.spot.outer_angle));
        light.spot.cast_shadows = s["cast_shadows"].as_bool(light.spot.cast_shadows);
    }

    // Animation
    light.animate = value["animate"].as_bool(false);
    light.animation_type = value["animation_type"].as_string();
    light.animation_speed = static_cast<float>(value["animation_speed"].as_float(1.0));
}

void SceneParser::parse_shadows(const TomlValue& value, ShadowsDef& shadows) {
    shadows.enabled = value["enabled"].as_bool(true);
    shadows.quality = parse_shadow_quality(value["quality"].as_string("medium"));
    shadows.filter = parse_shadow_filter(value["filter"].as_string("pcf"));
    shadows.map_size = static_cast<int>(value["map_size"].as_int(2048));
    shadows.bias = static_cast<float>(value["bias"].as_float(0.001));
    shadows.normal_bias = static_cast<float>(value["normal_bias"].as_float(0.01));
    shadows.max_distance = static_cast<float>(value["max_distance"].as_float(100.0));

    if (value.has("cascades")) {
        const auto& c = value["cascades"];
        shadows.cascades.count = static_cast<int>(c["count"].as_int(4));
        shadows.cascades.blend_distance = static_cast<float>(c["blend_distance"].as_float(5.0));
        shadows.cascades.stabilize = c["stabilize"].as_bool(true);
        if (c.has("splits")) {
            for (const auto& split : c["splits"].as_array()) {
                shadows.cascades.splits.push_back(static_cast<float>(split.as_float()));
            }
        }
    }

    shadows.contact_shadows = value["contact_shadows"].as_bool(false);
    shadows.contact_shadow_length = static_cast<float>(value["contact_shadow_length"].as_float(0.1));
}

void SceneParser::parse_environment(const TomlValue& value, EnvironmentDef& env) {
    // Sky
    if (value.has("sky")) {
        const auto& s = value["sky"];
        env.sky.type = parse_sky_type(s["type"].as_string("color"));
        env.sky.color = s["color"].as_color3(env.sky.color);
        env.sky.horizon_color = s["horizon_color"].as_color3(env.sky.horizon_color);
        env.sky.ground_color = s["ground_color"].as_color3(env.sky.ground_color);
        env.sky.texture = s["texture"].as_string();
        env.sky.rotation = static_cast<float>(s["rotation"].as_float(0.0));
        env.sky.exposure = static_cast<float>(s["exposure"].as_float(1.0));
        env.sky.sun_size = static_cast<float>(s["sun_size"].as_float(0.04));
        env.sky.atmosphere_density = static_cast<float>(s["atmosphere_density"].as_float(1.0));
    }

    // Fog
    if (value.has("fog")) {
        const auto& f = value["fog"];
        env.fog.enabled = f["enabled"].as_bool(false);
        env.fog.color = f["color"].as_color3(env.fog.color);
        env.fog.density = static_cast<float>(f["density"].as_float(0.01));
        env.fog.start = static_cast<float>(f["start"].as_float(10.0));
        env.fog.end = static_cast<float>(f["end"].as_float(100.0));
        env.fog.height_falloff = static_cast<float>(f["height_falloff"].as_float(0.5));
        env.fog.height_fog = f["height_fog"].as_bool(false);
    }

    // Ambient occlusion
    if (value.has("ambient_occlusion")) {
        const auto& ao = value["ambient_occlusion"];
        env.ambient_occlusion.enabled = ao["enabled"].as_bool(true);
        env.ambient_occlusion.intensity = static_cast<float>(ao["intensity"].as_float(1.0));
        env.ambient_occlusion.radius = static_cast<float>(ao["radius"].as_float(0.5));
        env.ambient_occlusion.bias = static_cast<float>(ao["bias"].as_float(0.025));
        env.ambient_occlusion.samples = static_cast<int>(ao["samples"].as_int(16));
        env.ambient_occlusion.temporal = ao["temporal"].as_bool(true);
    }

    env.ambient_color = value["ambient_color"].as_color3(env.ambient_color);
    env.ambient_intensity = static_cast<float>(value["ambient_intensity"].as_float(0.3));
    env.environment_map = value["environment_map"].as_string();
    env.environment_intensity = static_cast<float>(value["environment_intensity"].as_float(1.0));
    env.reflection_probe = value["reflection_probe"].as_string();
}

void SceneParser::parse_picking(const TomlValue& value, PickingDef& picking) {
    picking.enabled = value["enabled"].as_bool(false);
    picking.mode = parse_picking_mode(value["mode"].as_string("click"));
    picking.max_distance = static_cast<float>(value["max_distance"].as_float(1000.0));
    picking.highlight_on_hover = value["highlight_on_hover"].as_bool(true);
    picking.highlight_color = value["highlight_color"].as_color4(picking.highlight_color);

    if (value.has("layers")) {
        for (const auto& layer : value["layers"].as_array()) {
            picking.layers.push_back(layer.as_string());
        }
    }
}

void SceneParser::parse_spatial(const TomlValue& value, SpatialDef& spatial) {
    spatial.type = parse_spatial_type(value["type"].as_string("bvh"));
    spatial.max_objects_per_node = static_cast<int>(value["max_objects_per_node"].as_int(8));
    spatial.max_depth = static_cast<int>(value["max_depth"].as_int(16));
    spatial.world_bounds_min = value["world_bounds_min"].as_vec3(spatial.world_bounds_min);
    spatial.world_bounds_max = value["world_bounds_max"].as_vec3(spatial.world_bounds_max);
    spatial.grid_cell_size = static_cast<float>(value["grid_cell_size"].as_float(10.0));
    spatial.dynamic_update = value["dynamic_update"].as_bool(true);
}

void SceneParser::parse_debug(const TomlValue& value, DebugDef& debug) {
    debug.show_wireframe = value["show_wireframe"].as_bool(false);
    debug.show_normals = value["show_normals"].as_bool(false);
    debug.show_bounds = value["show_bounds"].as_bool(false);
    debug.show_colliders = value["show_colliders"].as_bool(false);
    debug.show_lights = value["show_lights"].as_bool(false);
    debug.show_cameras = value["show_cameras"].as_bool(false);
    debug.show_skeleton = value["show_skeleton"].as_bool(false);
    debug.show_navmesh = value["show_navmesh"].as_bool(false);
    debug.show_fps = value["show_fps"].as_bool(false);
    debug.show_stats = value["show_stats"].as_bool(false);
    debug.wireframe_color = value["wireframe_color"].as_color3(debug.wireframe_color);
    debug.bounds_color = value["bounds_color"].as_color3(debug.bounds_color);
    debug.collider_color = value["collider_color"].as_color3(debug.collider_color);
}

void SceneParser::parse_input(const TomlValue& value, InputConfig& input) {
    input.mouse_sensitivity = static_cast<float>(value["mouse_sensitivity"].as_float(1.0));
    input.gamepad_sensitivity = static_cast<float>(value["gamepad_sensitivity"].as_float(1.0));
    input.invert_y = value["invert_y"].as_bool(false);

    if (value.has("bindings")) {
        for (const auto& binding_value : value["bindings"].as_array()) {
            InputBindingDef binding;
            binding.action = binding_value["action"].as_string();

            if (binding_value.has("keys")) {
                for (const auto& key : binding_value["keys"].as_array()) {
                    binding.keys.push_back(key.as_string());
                }
            }
            if (binding_value.has("mouse_buttons")) {
                for (const auto& btn : binding_value["mouse_buttons"].as_array()) {
                    binding.mouse_buttons.push_back(btn.as_string());
                }
            }
            if (binding_value.has("gamepad_buttons")) {
                for (const auto& btn : binding_value["gamepad_buttons"].as_array()) {
                    binding.gamepad_buttons.push_back(btn.as_string());
                }
            }
            binding.gamepad_axis = binding_value["gamepad_axis"].as_string();
            binding.dead_zone = static_cast<float>(binding_value["dead_zone"].as_float(0.1));
            binding.invert = binding_value["invert"].as_bool(false);

            input.bindings.push_back(std::move(binding));
        }
    }
}

void SceneParser::parse_entity(const TomlValue& value, EntityDef& entity) {
    entity.name = value["name"].as_string();
    entity.prefab = value["prefab"].as_string();
    entity.parent = value["parent"].as_string();
    entity.layer = value["layer"].as_string("world");
    entity.active = value["active"].as_bool(true);

    // Tags
    if (value.has("tags")) {
        for (const auto& tag : value["tags"].as_array()) {
            entity.tags.push_back(tag.as_string());
        }
    }

    // Transform
    if (value.has("transform")) {
        parse_transform(value["transform"], entity.transform);
    } else {
        // Legacy format support
        entity.transform.position = value["position"].as_vec3(entity.transform.position);
        entity.transform.rotation = value["rotation"].as_vec3(entity.transform.rotation);
        entity.transform.scale = value["scale"].as_vec3(entity.transform.scale);
    }

    // Mesh
    if (value.has("mesh")) {
        MeshDef mesh;
        parse_mesh(value["mesh"], mesh);
        entity.mesh = std::move(mesh);
    }

    // Material
    if (value.has("material")) {
        MaterialDef material;
        parse_material(value["material"], material);
        entity.material = std::move(material);
    }

    // Animation
    if (value.has("animation")) {
        AnimationDef anim;
        parse_animation(value["animation"], anim);
        entity.animation = std::move(anim);
    }

    // Physics
    if (value.has("physics")) {
        PhysicsDef physics;
        parse_physics(value["physics"], physics);
        entity.physics = std::move(physics);
    }

    // Health
    if (value.has("health")) {
        HealthDef health;
        parse_health(value["health"], health);
        entity.health = std::move(health);
    }

    // Weapon
    if (value.has("weapon")) {
        WeaponDef weapon;
        parse_weapon(value["weapon"], weapon);
        entity.weapon = std::move(weapon);
    }

    // Inventory
    if (value.has("inventory")) {
        InventoryDef inventory;
        parse_inventory(value["inventory"], inventory);
        entity.inventory = std::move(inventory);
    }

    // AI
    if (value.has("ai")) {
        AiDef ai;
        parse_ai(value["ai"], ai);
        entity.ai = std::move(ai);
    }

    // Trigger
    if (value.has("trigger")) {
        TriggerDef trigger;
        parse_trigger(value["trigger"], trigger);
        entity.trigger = std::move(trigger);
    }

    // Script
    if (value.has("script")) {
        ScriptDef script;
        parse_script(value["script"], script);
        entity.script = std::move(script);
    }

    // LOD
    if (value.has("lod")) {
        LodDef lod;
        parse_lod(value["lod"], lod);
        entity.lod = std::move(lod);
    }

    // Render settings
    if (value.has("render")) {
        parse_render_settings(value["render"], entity.render_settings);
    }

    // Light attachment
    if (value.has("light")) {
        LightDef light;
        parse_light(value["light"], light);
        entity.light = std::move(light);
    }

    // Children
    if (value.has("children")) {
        for (const auto& child_value : value["children"].as_array()) {
            EntityDef child;
            parse_entity(child_value, child);
            entity.children.push_back(std::move(child));
        }
    }
}

void SceneParser::parse_transform(const TomlValue& value, TransformDef& transform) {
    transform.position = value["position"].as_vec3(transform.position);
    transform.rotation = value["rotation"].as_vec3(transform.rotation);
    transform.scale = value["scale"].as_vec3(transform.scale);

    if (value.has("quaternion")) {
        transform.quaternion = value["quaternion"].as_vec4(transform.quaternion);
        transform.use_quaternion = true;
    }
}

void SceneParser::parse_mesh(const TomlValue& value, MeshDef& mesh) {
    mesh.file = value["file"].as_string();
    mesh.primitive = parse_mesh_primitive(value["primitive"].as_string("none"));
    mesh.size = value["size"].as_vec3(mesh.size);
    mesh.radius = static_cast<float>(value["radius"].as_float(mesh.radius));
    mesh.height = static_cast<float>(value["height"].as_float(mesh.height));
    mesh.segments = static_cast<int>(value["segments"].as_int(mesh.segments));
    mesh.rings = static_cast<int>(value["rings"].as_int(mesh.rings));
    mesh.inner_radius = static_cast<float>(value["inner_radius"].as_float(mesh.inner_radius));
    mesh.outer_radius = static_cast<float>(value["outer_radius"].as_float(mesh.outer_radius));

    if (value.has("lod_files")) {
        for (const auto& lod : value["lod_files"].as_array()) {
            mesh.lod_files.push_back(lod.as_string());
        }
    }
    if (value.has("lod_distances")) {
        for (const auto& dist : value["lod_distances"].as_array()) {
            mesh.lod_distances.push_back(static_cast<float>(dist.as_float()));
        }
    }
}

void SceneParser::parse_material(const TomlValue& value, MaterialDef& material) {
    material.name = value["name"].as_string();
    material.shader = value["shader"].as_string();

    // Albedo
    if (value.has("albedo")) {
        if (value["albedo"].is_string()) {
            material.albedo.texture = value["albedo"].as_string();
            material.albedo.has_texture = true;
        } else {
            material.albedo.color = value["albedo"].as_color4(material.albedo.color);
        }
    }
    if (value.has("albedo_texture")) {
        material.albedo.texture = value["albedo_texture"].as_string();
        material.albedo.has_texture = true;
    }

    // Metallic
    if (value.has("metallic")) {
        if (value["metallic"].is_string()) {
            material.metallic.texture = value["metallic"].as_string();
            material.metallic.has_texture = true;
        } else {
            material.metallic.value = static_cast<float>(value["metallic"].as_float(0.0));
        }
    }

    // Roughness
    if (value.has("roughness")) {
        if (value["roughness"].is_string()) {
            material.roughness.texture = value["roughness"].as_string();
            material.roughness.has_texture = true;
        } else {
            material.roughness.value = static_cast<float>(value["roughness"].as_float(0.5));
        }
    }

    // Normal map
    material.normal_map = value["normal_map"].as_string();
    material.normal_scale = static_cast<float>(value["normal_scale"].as_float(1.0));

    // Occlusion
    material.occlusion_map = value["occlusion_map"].as_string();
    material.occlusion_strength = static_cast<float>(value["occlusion_strength"].as_float(1.0));

    // Emissive
    if (value.has("emissive")) {
        if (value["emissive"].is_string()) {
            material.emissive.texture = value["emissive"].as_string();
            material.emissive.has_texture = true;
        } else {
            material.emissive.color = value["emissive"].as_color4({0, 0, 0, 1});
        }
    }
    material.emissive_intensity = static_cast<float>(value["emissive_intensity"].as_float(1.0));

    // Alpha
    material.alpha_cutoff = static_cast<float>(value["alpha_cutoff"].as_float(0.5));
    material.alpha_blend = value["alpha_blend"].as_bool(false);
    material.double_sided = value["double_sided"].as_bool(false);

    // Transmission
    if (value.has("transmission")) {
        const auto& t = value["transmission"];
        material.transmission.enabled = true;
        material.transmission.factor = static_cast<float>(t["factor"].as_float(0.0));
        material.transmission.texture = t["texture"].as_string();
        material.transmission.ior = static_cast<float>(t["ior"].as_float(1.5));
        material.transmission.thickness = static_cast<float>(t["thickness"].as_float(0.0));
        material.transmission.attenuation_color = t["attenuation_color"].as_color3({1, 1, 1});
        material.transmission.attenuation_distance = static_cast<float>(t["attenuation_distance"].as_float(0.0));
    }

    // Sheen
    if (value.has("sheen")) {
        const auto& s = value["sheen"];
        material.sheen.enabled = true;
        material.sheen.color = s["color"].as_color3({0, 0, 0});
        material.sheen.roughness = static_cast<float>(s["roughness"].as_float(0.0));
        material.sheen.color_texture = s["color_texture"].as_string();
        material.sheen.roughness_texture = s["roughness_texture"].as_string();
    }

    // Clearcoat
    if (value.has("clearcoat")) {
        const auto& c = value["clearcoat"];
        material.clearcoat.enabled = true;
        material.clearcoat.factor = static_cast<float>(c["factor"].as_float(0.0));
        material.clearcoat.roughness = static_cast<float>(c["roughness"].as_float(0.0));
        material.clearcoat.texture = c["texture"].as_string();
        material.clearcoat.roughness_texture = c["roughness_texture"].as_string();
        material.clearcoat.normal_texture = c["normal_texture"].as_string();
    }

    // Anisotropy
    if (value.has("anisotropy")) {
        const auto& a = value["anisotropy"];
        material.anisotropy.enabled = true;
        material.anisotropy.strength = static_cast<float>(a["strength"].as_float(0.0));
        material.anisotropy.rotation = static_cast<float>(a["rotation"].as_float(0.0));
        material.anisotropy.texture = a["texture"].as_string();
        material.anisotropy.direction_texture = a["direction_texture"].as_string();
    }

    // Subsurface
    if (value.has("subsurface")) {
        const auto& ss = value["subsurface"];
        material.subsurface.enabled = true;
        material.subsurface.factor = static_cast<float>(ss["factor"].as_float(0.0));
        material.subsurface.color = ss["color"].as_color3({1, 0.2f, 0.1f});
        material.subsurface.radius = static_cast<float>(ss["radius"].as_float(1.0));
        material.subsurface.texture = ss["texture"].as_string();
    }

    // Iridescence
    if (value.has("iridescence")) {
        const auto& i = value["iridescence"];
        material.iridescence.enabled = true;
        material.iridescence.factor = static_cast<float>(i["factor"].as_float(0.0));
        material.iridescence.ior = static_cast<float>(i["ior"].as_float(1.3));
        material.iridescence.thickness_min = static_cast<float>(i["thickness_min"].as_float(100.0));
        material.iridescence.thickness_max = static_cast<float>(i["thickness_max"].as_float(400.0));
        material.iridescence.texture = i["texture"].as_string();
        material.iridescence.thickness_texture = i["thickness_texture"].as_string();
    }

    // Height/parallax
    material.height_map = value["height_map"].as_string();
    material.height_scale = static_cast<float>(value["height_scale"].as_float(0.1));
    material.parallax_occlusion = value["parallax_occlusion"].as_bool(false);

    // Detail textures
    material.detail_albedo = value["detail_albedo"].as_string();
    material.detail_normal = value["detail_normal"].as_string();
    material.detail_scale = value["detail_scale"].as_vec2(material.detail_scale);
}

void SceneParser::parse_animation(const TomlValue& value, AnimationDef& anim) {
    anim.type = parse_animation_type(value["type"].as_string("none"));
    anim.enabled = value["enabled"].as_bool(true);
    anim.play_on_start = value["play_on_start"].as_bool(true);
    anim.speed = static_cast<float>(value["speed"].as_float(1.0));
    anim.loop = value["loop"].as_bool(true);
    anim.blend_time = static_cast<float>(value["blend_time"].as_float(0.2));

    // Rotate
    if (value.has("rotate")) {
        const auto& r = value["rotate"];
        anim.rotate.axis = r["axis"].as_vec3(anim.rotate.axis);
        anim.rotate.speed = static_cast<float>(r["speed"].as_float(1.0));
        anim.rotate.local_space = r["local_space"].as_bool(true);
    }

    // Oscillate
    if (value.has("oscillate")) {
        const auto& o = value["oscillate"];
        anim.oscillate.axis = o["axis"].as_vec3(anim.oscillate.axis);
        anim.oscillate.amplitude = static_cast<float>(o["amplitude"].as_float(1.0));
        anim.oscillate.frequency = static_cast<float>(o["frequency"].as_float(1.0));
        anim.oscillate.phase = static_cast<float>(o["phase"].as_float(0.0));
        anim.oscillate.easing = parse_animation_easing(o["easing"].as_string("linear"));
    }

    // Orbit
    if (value.has("orbit")) {
        const auto& o = value["orbit"];
        anim.orbit.center = o["center"].as_vec3(anim.orbit.center);
        anim.orbit.axis = o["axis"].as_vec3(anim.orbit.axis);
        anim.orbit.radius = static_cast<float>(o["radius"].as_float(5.0));
        anim.orbit.speed = static_cast<float>(o["speed"].as_float(1.0));
        anim.orbit.face_center = o["face_center"].as_bool(true);
    }

    // Pulse
    if (value.has("pulse")) {
        const auto& p = value["pulse"];
        anim.pulse.scale_min = p["scale_min"].as_vec3(anim.pulse.scale_min);
        anim.pulse.scale_max = p["scale_max"].as_vec3(anim.pulse.scale_max);
        anim.pulse.frequency = static_cast<float>(p["frequency"].as_float(1.0));
        anim.pulse.easing = parse_animation_easing(p["easing"].as_string("ease_in_out"));
    }

    // Path
    if (value.has("path") || value.has("waypoints")) {
        const auto& path_value = value.has("path") ? value["path"] : value;
        anim.path.loop = path_value["loop"].as_bool(true);
        anim.path.ping_pong = path_value["ping_pong"].as_bool(false);
        anim.path.duration = static_cast<float>(path_value["duration"].as_float(1.0));
        anim.path.orient_to_path = path_value["orient_to_path"].as_bool(false);

        if (path_value.has("waypoints")) {
            for (const auto& wp : path_value["waypoints"].as_array()) {
                PathWaypoint waypoint;
                waypoint.position = wp["position"].as_vec3();
                waypoint.rotation = wp["rotation"].as_vec4(waypoint.rotation);
                waypoint.time = static_cast<float>(wp["time"].as_float(0.0));
                waypoint.easing = parse_animation_easing(wp["easing"].as_string("linear"));
                anim.path.waypoints.push_back(std::move(waypoint));
            }
        }
    }

    // Skeletal
    anim.animation_file = value["animation_file"].as_string();
    anim.animation_name = value["animation_name"].as_string();
}

void SceneParser::parse_physics(const TomlValue& value, PhysicsDef& physics) {
    physics.body_type = parse_physics_body_type(value["body_type"].as_string("static"));
    physics.mass = static_cast<float>(value["mass"].as_float(1.0));
    physics.linear_damping = static_cast<float>(value["linear_damping"].as_float(0.0));
    physics.angular_damping = static_cast<float>(value["angular_damping"].as_float(0.05));
    physics.center_of_mass = value["center_of_mass"].as_vec3(physics.center_of_mass);
    physics.use_gravity = value["use_gravity"].as_bool(true);
    physics.is_kinematic = value["is_kinematic"].as_bool(false);
    physics.continuous_collision = value["continuous_collision"].as_bool(false);

    // Colliders
    if (value.has("colliders")) {
        for (const auto& coll_value : value["colliders"].as_array()) {
            ColliderDef collider;
            parse_collider(coll_value, collider);
            physics.colliders.push_back(std::move(collider));
        }
    } else if (value.has("collider")) {
        ColliderDef collider;
        parse_collider(value["collider"], collider);
        physics.colliders.push_back(std::move(collider));
    }

    // Collision groups
    if (value.has("collision_groups")) {
        const auto& cg = value["collision_groups"];
        physics.collision_groups.group = static_cast<std::uint32_t>(cg["group"].as_int(1));
        physics.collision_groups.mask = static_cast<std::uint32_t>(cg["mask"].as_int(0xFFFFFFFF));

        if (cg.has("collides_with")) {
            for (const auto& name : cg["collides_with"].as_array()) {
                physics.collision_groups.collides_with.push_back(name.as_string());
            }
        }
        if (cg.has("ignores")) {
            for (const auto& name : cg["ignores"].as_array()) {
                physics.collision_groups.ignores.push_back(name.as_string());
            }
        }
    }

    // Joints
    if (value.has("joints")) {
        for (const auto& joint_value : value["joints"].as_array()) {
            JointDef joint;
            joint.type = parse_joint_type(joint_value["type"].as_string("fixed"));
            joint.connected_body = joint_value["connected_body"].as_string();
            joint.anchor = joint_value["anchor"].as_vec3(joint.anchor);
            joint.connected_anchor = joint_value["connected_anchor"].as_vec3(joint.connected_anchor);
            joint.axis = joint_value["axis"].as_vec3(joint.axis);
            joint.min_limit = static_cast<float>(joint_value["min_limit"].as_float(0.0));
            joint.max_limit = static_cast<float>(joint_value["max_limit"].as_float(0.0));
            joint.spring_stiffness = static_cast<float>(joint_value["spring_stiffness"].as_float(0.0));
            joint.spring_damping = static_cast<float>(joint_value["spring_damping"].as_float(0.0));
            joint.enable_collision = joint_value["enable_collision"].as_bool(false);
            joint.break_force = static_cast<float>(joint_value["break_force"].as_float(-1.0));
            joint.break_torque = static_cast<float>(joint_value["break_torque"].as_float(-1.0));
            physics.joints.push_back(std::move(joint));
        }
    }

    // Character controller
    if (value.has("character_controller")) {
        CharacterControllerDef cc;
        const auto& ccv = value["character_controller"];
        cc.height = static_cast<float>(ccv["height"].as_float(1.8));
        cc.radius = static_cast<float>(ccv["radius"].as_float(0.3));
        cc.step_offset = static_cast<float>(ccv["step_offset"].as_float(0.3));
        cc.slope_limit = static_cast<float>(ccv["slope_limit"].as_float(45.0));
        cc.skin_width = static_cast<float>(ccv["skin_width"].as_float(0.02));
        cc.center = ccv["center"].as_vec3(cc.center);
        physics.character_controller = std::move(cc);
    }

    // Constraints
    physics.freeze_position_x = value["freeze_position_x"].as_bool(false);
    physics.freeze_position_y = value["freeze_position_y"].as_bool(false);
    physics.freeze_position_z = value["freeze_position_z"].as_bool(false);
    physics.freeze_rotation_x = value["freeze_rotation_x"].as_bool(false);
    physics.freeze_rotation_y = value["freeze_rotation_y"].as_bool(false);
    physics.freeze_rotation_z = value["freeze_rotation_z"].as_bool(false);
}

void SceneParser::parse_collider(const TomlValue& value, ColliderDef& collider) {
    collider.shape = parse_collider_shape(value["shape"].as_string("box"));
    collider.size = value["size"].as_vec3(collider.size);
    collider.radius = static_cast<float>(value["radius"].as_float(0.5));
    collider.height = static_cast<float>(value["height"].as_float(1.0));
    collider.capsule_axis = parse_capsule_axis(value["capsule_axis"].as_string("y"));
    collider.offset = value["offset"].as_vec3(collider.offset);
    collider.rotation = value["rotation"].as_vec4(collider.rotation);
    collider.mesh = value["mesh"].as_string();
    collider.is_trigger = value["is_trigger"].as_bool(false);

    if (value.has("material")) {
        const auto& m = value["material"];
        collider.material.friction = static_cast<float>(m["friction"].as_float(0.5));
        collider.material.restitution = static_cast<float>(m["restitution"].as_float(0.3));
        collider.material.density = static_cast<float>(m["density"].as_float(1.0));
    }
}

void SceneParser::parse_health(const TomlValue& value, HealthDef& health) {
    health.max_health = static_cast<float>(value["max_health"].as_float(100.0));
    health.current_health = static_cast<float>(value["current_health"].as_float(health.max_health));
    health.max_shields = static_cast<float>(value["max_shields"].as_float(0.0));
    health.current_shields = static_cast<float>(value["current_shields"].as_float(health.max_shields));
    health.max_armor = static_cast<float>(value["max_armor"].as_float(0.0));
    health.current_armor = static_cast<float>(value["current_armor"].as_float(health.max_armor));
    health.health_regen = static_cast<float>(value["health_regen"].as_float(0.0));
    health.shield_regen = static_cast<float>(value["shield_regen"].as_float(0.0));
    health.regen_delay = static_cast<float>(value["regen_delay"].as_float(3.0));
    health.invulnerable = value["invulnerable"].as_bool(false);
    health.invulnerability_time = static_cast<float>(value["invulnerability_time"].as_float(0.0));
}

void SceneParser::parse_weapon(const TomlValue& value, WeaponDef& weapon) {
    weapon.name = value["name"].as_string();
    weapon.type = parse_weapon_type(value["type"].as_string("hitscan"));
    weapon.damage = static_cast<float>(value["damage"].as_float(10.0));
    weapon.fire_rate = static_cast<float>(value["fire_rate"].as_float(10.0));
    weapon.range = static_cast<float>(value["range"].as_float(100.0));
    weapon.spread = static_cast<float>(value["spread"].as_float(0.0));
    weapon.magazine_size = static_cast<int>(value["magazine_size"].as_int(30));
    weapon.current_ammo = static_cast<int>(value["current_ammo"].as_int(weapon.magazine_size));
    weapon.reserve_ammo = static_cast<int>(value["reserve_ammo"].as_int(90));
    weapon.reload_time = static_cast<float>(value["reload_time"].as_float(2.0));
    weapon.damage_type = value["damage_type"].as_string("physical");

    // Projectile
    weapon.projectile_speed = static_cast<float>(value["projectile_speed"].as_float(50.0));
    weapon.projectile_gravity = static_cast<float>(value["projectile_gravity"].as_float(0.0));
    weapon.projectile_prefab = value["projectile_prefab"].as_string();

    // Melee
    weapon.melee_arc = static_cast<float>(value["melee_arc"].as_float(90.0));
    weapon.attack_duration = static_cast<float>(value["attack_duration"].as_float(0.5));

    // Effects
    weapon.fire_sound = value["fire_sound"].as_string();
    weapon.reload_sound = value["reload_sound"].as_string();
    weapon.impact_effect = value["impact_effect"].as_string();
    weapon.muzzle_flash = value["muzzle_flash"].as_string();
    weapon.recoil = value["recoil"].as_vec3(weapon.recoil);
}

void SceneParser::parse_inventory(const TomlValue& value, InventoryDef& inventory) {
    inventory.max_slots = static_cast<int>(value["max_slots"].as_int(20));
    inventory.max_weight = static_cast<float>(value["max_weight"].as_float(100.0));

    if (value.has("starting_items")) {
        for (const auto& item_value : value["starting_items"].as_array()) {
            InventorySlotDef slot;
            slot.item_id = item_value["item"].as_string();
            slot.count = static_cast<int>(item_value["count"].as_int(1));
            inventory.starting_items.push_back(std::move(slot));
        }
    }
}

void SceneParser::parse_ai(const TomlValue& value, AiDef& ai) {
    ai.behavior = parse_ai_behavior(value["behavior"].as_string("idle"));
    ai.detection_range = static_cast<float>(value["detection_range"].as_float(20.0));
    ai.attack_range = static_cast<float>(value["attack_range"].as_float(5.0));
    ai.fov = static_cast<float>(value["fov"].as_float(120.0));
    ai.move_speed = static_cast<float>(value["move_speed"].as_float(3.0));
    ai.turn_speed = static_cast<float>(value["turn_speed"].as_float(180.0));
    ai.target_tag = value["target_tag"].as_string();
    ai.behavior_tree = value["behavior_tree"].as_string();
    ai.blackboard_preset = value["blackboard_preset"].as_string();

    if (value.has("patrol_points")) {
        for (const auto& point : value["patrol_points"].as_array()) {
            ai.patrol_points.push_back(point.as_vec3());
        }
    }
}

void SceneParser::parse_trigger(const TomlValue& value, TriggerDef& trigger) {
    trigger.shape = parse_collider_shape(value["shape"].as_string("box"));
    trigger.size = value["size"].as_vec3(trigger.size);
    trigger.radius = static_cast<float>(value["radius"].as_float(1.0));
    trigger.once = value["once"].as_bool(false);
    trigger.cooldown = static_cast<float>(value["cooldown"].as_float(0.0));

    if (value.has("filter_tags")) {
        for (const auto& tag : value["filter_tags"].as_array()) {
            trigger.filter_tags.push_back(tag.as_string());
        }
    }

    auto parse_actions = [](const TomlValue& arr, std::vector<TriggerActionDef>& actions) {
        for (const auto& action_value : arr.as_array()) {
            TriggerActionDef action;
            action.type = action_value["type"].as_string();
            action.target = action_value["target"].as_string();
            // Parameters would need more complex parsing
            actions.push_back(std::move(action));
        }
    };

    if (value.has("on_enter")) parse_actions(value["on_enter"], trigger.on_enter);
    if (value.has("on_exit")) parse_actions(value["on_exit"], trigger.on_exit);
    if (value.has("on_stay")) parse_actions(value["on_stay"], trigger.on_stay);
}

void SceneParser::parse_script(const TomlValue& value, ScriptDef& script) {
    script.cpp_class = value["cpp_class"].as_string();
    script.blueprint = value["blueprint"].as_string();
    script.voidscript = value["voidscript"].as_string();
    script.wasm_module = value["wasm_module"].as_string();

    // Event bindings
    if (value.has("events")) {
        for (const auto& event_value : value["events"].as_array()) {
            EventBindingDef binding;
            binding.event_name = event_value["event"].as_string();
            binding.handler = event_value["handler"].as_string();
            script.event_bindings.push_back(std::move(binding));
        }
    }
}

void SceneParser::parse_lod(const TomlValue& value, LodDef& lod) {
    lod.bias = static_cast<float>(value["bias"].as_float(0.0));
    lod.fade_transition = value["fade_transition"].as_bool(true);
    lod.fade_duration = static_cast<float>(value["fade_duration"].as_float(0.2));

    if (value.has("levels")) {
        for (const auto& level_value : value["levels"].as_array()) {
            LodLevelDef level;
            level.mesh = level_value["mesh"].as_string();
            level.distance = static_cast<float>(level_value["distance"].as_float(0.0));
            level.screen_size = static_cast<float>(level_value["screen_size"].as_float(1.0));
            lod.levels.push_back(std::move(level));
        }
    }
}

void SceneParser::parse_render_settings(const TomlValue& value, RenderSettingsDef& settings) {
    settings.visible = value["visible"].as_bool(true);
    settings.cast_shadows = value["cast_shadows"].as_bool(true);
    settings.receive_shadows = value["receive_shadows"].as_bool(true);
    settings.static_object = value["static"].as_bool(false);
    settings.render_order = static_cast<int>(value["render_order"].as_int(0));
    settings.render_layer = value["render_layer"].as_string();
}

void SceneParser::parse_particle_emitter(const TomlValue& value, ParticleEmitterDef& emitter) {
    emitter.name = value["name"].as_string();
    emitter.position = value["position"].as_vec3(emitter.position);
    emitter.enabled = value["enabled"].as_bool(true);
    emitter.layer = value["layer"].as_string("particles");

    emitter.shape = parse_emission_shape(value["shape"].as_string("point"));
    emitter.emission_rate = static_cast<float>(value["emission_rate"].as_float(10.0));
    emitter.max_particles = static_cast<int>(value["max_particles"].as_int(1000));
    emitter.shape_size = value["shape_size"].as_vec3(emitter.shape_size);
    emitter.shape_radius = static_cast<float>(value["shape_radius"].as_float(1.0));
    emitter.shape_angle = static_cast<float>(value["shape_angle"].as_float(45.0));

    emitter.lifetime_min = static_cast<float>(value["lifetime_min"].as_float(1.0));
    emitter.lifetime_max = static_cast<float>(value["lifetime_max"].as_float(2.0));
    emitter.speed_min = static_cast<float>(value["speed_min"].as_float(1.0));
    emitter.speed_max = static_cast<float>(value["speed_max"].as_float(5.0));
    emitter.size_min = static_cast<float>(value["size_min"].as_float(0.1));
    emitter.size_max = static_cast<float>(value["size_max"].as_float(0.5));
    emitter.color_start = value["color_start"].as_color4(emitter.color_start);
    emitter.color_end = value["color_end"].as_color4(emitter.color_end);

    emitter.gravity = value["gravity"].as_vec3(emitter.gravity);
    emitter.drag = static_cast<float>(value["drag"].as_float(0.0));
    emitter.world_space = value["world_space"].as_bool(true);

    emitter.texture = value["texture"].as_string();
    emitter.material = value["material"].as_string();
    emitter.additive_blend = value["additive_blend"].as_bool(false);
    emitter.face_camera = value["face_camera"].as_bool(true);

    emitter.texture_rows = static_cast<int>(value["texture_rows"].as_int(1));
    emitter.texture_cols = static_cast<int>(value["texture_cols"].as_int(1));
    emitter.animation_speed = static_cast<float>(value["animation_speed"].as_float(1.0));
    emitter.random_start_frame = value["random_start_frame"].as_bool(false);
}

void SceneParser::parse_texture(const TomlValue& value, TextureDef& texture) {
    texture.name = value["name"].as_string();
    texture.path = value["path"].as_string();
    texture.filter = parse_texture_filter(value["filter"].as_string("linear"));
    texture.wrap = parse_texture_wrap(value["wrap"].as_string("repeat"));
    texture.generate_mips = value["generate_mips"].as_bool(true);
    texture.srgb = value["srgb"].as_bool(true);
    texture.max_anisotropy = static_cast<int>(value["max_anisotropy"].as_int(8));
}

void SceneParser::parse_item(const TomlValue& value, ItemDef& item) {
    item.id = value["id"].as_string();
    item.name = value["name"].as_string();
    item.description = value["description"].as_string();
    item.type = parse_item_type(value["item_type"].as_string("misc"));
    item.rarity = parse_item_rarity(value["rarity"].as_string("common"));
    item.max_stack = static_cast<int>(value["max_stack"].as_int(1));
    item.weight = static_cast<float>(value["weight"].as_float(0.0));
    item.value = static_cast<int>(value["value"].as_int(0));
    item.icon = value["icon"].as_string();
    item.model = value["model"].as_string();

    // Consumable
    if (value.has("consumable")) {
        const auto& c = value["consumable"];
        item.use_time = static_cast<float>(c["use_time"].as_float(0.0));
        item.use_animation = c["animation"].as_string();

        if (c.has("effects")) {
            for (const auto& effect_value : c["effects"].as_array()) {
                ConsumableEffectDef effect;
                effect.type = effect_value["type"].as_string();
                effect.amount = static_cast<float>(effect_value["amount"].as_float(0.0));
                effect.duration = static_cast<float>(effect_value["duration"].as_float(0.0));
                effect.status_effect = effect_value["status_effect"].as_string();
                item.effects.push_back(std::move(effect));
            }
        }
    }

    // Equipment
    item.slot = value["slot"].as_string();
    if (value.has("stats")) {
        for (const auto& [stat_name, stat_value] : value["stats"].as_table()) {
            item.stats[stat_name] = static_cast<float>(stat_value.as_float(0.0));
        }
    }
}

void SceneParser::parse_status_effect(const TomlValue& value, StatusEffectDef& effect) {
    effect.name = value["name"].as_string();
    effect.type = parse_status_effect_type(value["type"].as_string("buff"));
    effect.duration = static_cast<float>(value["duration"].as_float(5.0));
    effect.tick_rate = static_cast<float>(value["tick_rate"].as_float(1.0));
    effect.stacks = value["stacks"].as_bool(false);
    effect.max_stacks = static_cast<int>(value["max_stacks"].as_int(1));
    effect.icon = value["icon"].as_string();

    if (value.has("effects")) {
        for (const auto& eff_value : value["effects"].as_array()) {
            ConsumableEffectDef eff;
            eff.type = eff_value["type"].as_string();
            eff.amount = static_cast<float>(eff_value["amount"].as_float(0.0));
            if (eff_value.has("damage")) {
                eff.amount = static_cast<float>(eff_value["damage"].as_float(0.0));
            }
            if (eff_value.has("multiplier")) {
                eff.amount = static_cast<float>(eff_value["multiplier"].as_float(1.0));
            }
            eff.duration = static_cast<float>(eff_value["duration"].as_float(0.0));
            effect.effects.push_back(std::move(eff));
        }
    }
}

void SceneParser::parse_quest(const TomlValue& value, QuestDef& quest) {
    quest.id = value["id"].as_string();
    quest.name = value["name"].as_string();
    quest.description = value["description"].as_string();
    quest.auto_start = value["auto_start"].as_bool(false);
    quest.on_complete_event = value["on_complete_event"].as_string();

    if (value.has("prerequisites")) {
        for (const auto& prereq : value["prerequisites"].as_array()) {
            quest.prerequisites.push_back(prereq.as_string());
        }
    }

    if (value.has("objectives")) {
        for (const auto& obj_value : value["objectives"].as_array()) {
            QuestObjectiveDef obj;
            obj.id = obj_value["id"].as_string();
            obj.description = obj_value["description"].as_string();
            obj.type = parse_objective_type(obj_value["type"].as_string("custom"));
            obj.target = obj_value["target"].as_string();
            obj.count = static_cast<int>(obj_value["count"].as_int(1));
            obj.optional = obj_value["optional"].as_bool(false);
            obj.marker = obj_value["marker"].as_string();
            quest.objectives.push_back(std::move(obj));
        }
    }

    if (value.has("rewards")) {
        for (const auto& reward_value : value["rewards"].as_array()) {
            QuestRewardDef reward;
            reward.type = reward_value["type"].as_string();
            reward.item = reward_value["item"].as_string();
            reward.count = static_cast<int>(reward_value["count"].as_int(1));
            reward.xp = static_cast<int>(reward_value["xp"].as_int(0));
            reward.currency = static_cast<int>(reward_value["currency"].as_int(0));
            quest.rewards.push_back(std::move(reward));
        }
    }
}

void SceneParser::parse_loot_table(const TomlValue& value, LootTableDef& loot) {
    loot.id = value["id"].as_string();
    loot.rolls = static_cast<int>(value["rolls"].as_int(1));
    loot.allow_duplicates = value["allow_duplicates"].as_bool(false);

    if (value.has("entries")) {
        for (const auto& entry_value : value["entries"].as_array()) {
            LootEntryDef entry;
            entry.item_id = entry_value["item"].as_string();
            entry.weight = static_cast<float>(entry_value["weight"].as_float(1.0));
            entry.count_min = static_cast<int>(entry_value["count_min"].as_int(1));
            entry.count_max = static_cast<int>(entry_value["count_max"].as_int(1));
            loot.entries.push_back(std::move(entry));
        }
    }
}

void SceneParser::parse_audio_config(const TomlValue& value, AudioConfigDef& audio) {
    audio.default_music = value["default_music"].as_string();
    audio.master_volume = static_cast<float>(value["master_volume"].as_float(1.0));

    if (value.has("ambient")) {
        for (const auto& amb_value : value["ambient"].as_array()) {
            AmbientSoundDef amb;
            amb.name = amb_value["name"].as_string();
            amb.file = amb_value["file"].as_string();
            amb.volume = static_cast<float>(amb_value["volume"].as_float(1.0));
            amb.loop = amb_value["loop"].as_bool(true);
            amb.position = amb_value["position"].as_vec3(amb.position);
            amb.min_distance = static_cast<float>(amb_value["min_distance"].as_float(1.0));
            amb.max_distance = static_cast<float>(amb_value["max_distance"].as_float(50.0));
            amb.spatial = amb_value["spatial"].as_bool(true);
            audio.ambient.push_back(std::move(amb));
        }
    }

    if (value.has("music")) {
        for (const auto& music_value : value["music"].as_array()) {
            MusicTrackDef track;
            track.name = music_value["name"].as_string();
            track.file = music_value["file"].as_string();
            track.volume = static_cast<float>(music_value["volume"].as_float(1.0));
            track.loop = music_value["loop"].as_bool(true);
            track.fade_in = static_cast<float>(music_value["fade_in"].as_float(1.0));
            track.fade_out = static_cast<float>(music_value["fade_out"].as_float(1.0));
            audio.music.push_back(std::move(track));
        }
    }

    if (value.has("reverb_zones")) {
        for (const auto& reverb_value : value["reverb_zones"].as_array()) {
            ReverbZoneDef reverb;
            reverb.name = reverb_value["name"].as_string();
            reverb.position = reverb_value["position"].as_vec3(reverb.position);
            reverb.size = reverb_value["size"].as_vec3(reverb.size);
            reverb.preset = reverb_value["preset"].as_string();
            reverb.mix = static_cast<float>(reverb_value["mix"].as_float(1.0));
            audio.reverb_zones.push_back(std::move(reverb));
        }
    }
}

void SceneParser::parse_navigation_config(const TomlValue& value, NavigationConfigDef& nav) {
    nav.auto_generate = value["auto_generate"].as_bool(true);
    nav.realtime_update = value["realtime_update"].as_bool(false);

    if (value.has("navmesh")) {
        const auto& nm = value["navmesh"];
        nav.navmesh.agent_radius = static_cast<float>(nm["agent_radius"].as_float(0.5));
        nav.navmesh.agent_height = static_cast<float>(nm["agent_height"].as_float(2.0));
        nav.navmesh.max_slope = static_cast<float>(nm["max_slope"].as_float(45.0));
        nav.navmesh.step_height = static_cast<float>(nm["step_height"].as_float(0.3));
        nav.navmesh.cell_size = static_cast<float>(nm["cell_size"].as_float(0.3));
        nav.navmesh.cell_height = static_cast<float>(nm["cell_height"].as_float(0.2));

        if (nm.has("walkable_layers")) {
            for (const auto& layer : nm["walkable_layers"].as_array()) {
                nav.navmesh.walkable_layers.push_back(layer.as_string());
            }
        }
    }

    if (value.has("areas")) {
        for (const auto& area_value : value["areas"].as_array()) {
            NavAreaDef area;
            area.name = area_value["name"].as_string();
            area.cost = static_cast<float>(area_value["cost"].as_float(1.0));
            area.color = area_value["color"].as_color3(area.color);
            nav.areas.push_back(std::move(area));
        }
    }
}

// =============================================================================
// Enum Parsers
// =============================================================================

CameraType SceneParser::parse_camera_type(const std::string& str) {
    if (str == "orthographic") return CameraType::Orthographic;
    return CameraType::Perspective;
}

CameraControlMode SceneParser::parse_camera_control_mode(const std::string& str) {
    if (str == "fps") return CameraControlMode::Fps;
    if (str == "orbit") return CameraControlMode::Orbit;
    if (str == "fly") return CameraControlMode::Fly;
    if (str == "follow") return CameraControlMode::Follow;
    if (str == "rail") return CameraControlMode::Rail;
    if (str == "cinematic") return CameraControlMode::Cinematic;
    return CameraControlMode::None;
}

LightType SceneParser::parse_light_type(const std::string& str) {
    if (str == "directional") return LightType::Directional;
    if (str == "spot") return LightType::Spot;
    if (str == "area") return LightType::Area;
    if (str == "hemisphere") return LightType::Hemisphere;
    return LightType::Point;
}

ShadowQuality SceneParser::parse_shadow_quality(const std::string& str) {
    if (str == "off") return ShadowQuality::Off;
    if (str == "low") return ShadowQuality::Low;
    if (str == "high") return ShadowQuality::High;
    if (str == "ultra") return ShadowQuality::Ultra;
    return ShadowQuality::Medium;
}

ShadowFilter SceneParser::parse_shadow_filter(const std::string& str) {
    if (str == "none") return ShadowFilter::None;
    if (str == "pcss") return ShadowFilter::PCSS;
    if (str == "vsm") return ShadowFilter::VSM;
    if (str == "esm") return ShadowFilter::ESM;
    return ShadowFilter::PCF;
}

SkyType SceneParser::parse_sky_type(const std::string& str) {
    if (str == "none") return SkyType::None;
    if (str == "gradient") return SkyType::Gradient;
    if (str == "skybox") return SkyType::Skybox;
    if (str == "procedural") return SkyType::Procedural;
    if (str == "hdri") return SkyType::HDRI;
    return SkyType::Color;
}

MeshPrimitive SceneParser::parse_mesh_primitive(const std::string& str) {
    if (str == "cube") return MeshPrimitive::Cube;
    if (str == "sphere") return MeshPrimitive::Sphere;
    if (str == "cylinder") return MeshPrimitive::Cylinder;
    if (str == "capsule") return MeshPrimitive::Capsule;
    if (str == "cone") return MeshPrimitive::Cone;
    if (str == "plane") return MeshPrimitive::Plane;
    if (str == "quad") return MeshPrimitive::Quad;
    if (str == "torus") return MeshPrimitive::Torus;
    if (str == "custom") return MeshPrimitive::Custom;
    return MeshPrimitive::None;
}

AnimationType SceneParser::parse_animation_type(const std::string& str) {
    if (str == "rotate") return AnimationType::Rotate;
    if (str == "oscillate") return AnimationType::Oscillate;
    if (str == "path") return AnimationType::Path;
    if (str == "orbit") return AnimationType::Orbit;
    if (str == "pulse") return AnimationType::Pulse;
    if (str == "skeletal") return AnimationType::Skeletal;
    if (str == "morph") return AnimationType::Morph;
    return AnimationType::None;
}

AnimationEasing SceneParser::parse_animation_easing(const std::string& str) {
    if (str == "ease_in") return AnimationEasing::EaseIn;
    if (str == "ease_out") return AnimationEasing::EaseOut;
    if (str == "ease_in_out") return AnimationEasing::EaseInOut;
    if (str == "bounce") return AnimationEasing::Bounce;
    if (str == "elastic") return AnimationEasing::Elastic;
    return AnimationEasing::Linear;
}

PhysicsBodyType SceneParser::parse_physics_body_type(const std::string& str) {
    if (str == "dynamic") return PhysicsBodyType::Dynamic;
    if (str == "kinematic") return PhysicsBodyType::Kinematic;
    return PhysicsBodyType::Static;
}

ColliderShape SceneParser::parse_collider_shape(const std::string& str) {
    if (str == "sphere") return ColliderShape::Sphere;
    if (str == "capsule") return ColliderShape::Capsule;
    if (str == "cylinder") return ColliderShape::Cylinder;
    if (str == "mesh") return ColliderShape::Mesh;
    if (str == "convex") return ColliderShape::Convex;
    if (str == "compound") return ColliderShape::Compound;
    return ColliderShape::Box;
}

CapsuleAxis SceneParser::parse_capsule_axis(const std::string& str) {
    if (str == "x") return CapsuleAxis::X;
    if (str == "z") return CapsuleAxis::Z;
    return CapsuleAxis::Y;
}

JointType SceneParser::parse_joint_type(const std::string& str) {
    if (str == "hinge") return JointType::Hinge;
    if (str == "slider") return JointType::Slider;
    if (str == "ball") return JointType::Ball;
    if (str == "distance") return JointType::Distance;
    if (str == "cone") return JointType::Cone;
    if (str == "spring") return JointType::Spring;
    return JointType::Fixed;
}

EmissionShape SceneParser::parse_emission_shape(const std::string& str) {
    if (str == "sphere") return EmissionShape::Sphere;
    if (str == "hemisphere") return EmissionShape::Hemisphere;
    if (str == "cone") return EmissionShape::Cone;
    if (str == "box") return EmissionShape::Box;
    if (str == "circle") return EmissionShape::Circle;
    if (str == "edge") return EmissionShape::Edge;
    if (str == "mesh") return EmissionShape::Mesh;
    return EmissionShape::Point;
}

WeaponType SceneParser::parse_weapon_type(const std::string& str) {
    if (str == "projectile") return WeaponType::Projectile;
    if (str == "melee") return WeaponType::Melee;
    if (str == "beam") return WeaponType::Beam;
    if (str == "area") return WeaponType::Area;
    return WeaponType::Hitscan;
}

AiBehavior SceneParser::parse_ai_behavior(const std::string& str) {
    if (str == "patrol") return AiBehavior::Patrol;
    if (str == "guard") return AiBehavior::Guard;
    if (str == "follow") return AiBehavior::Follow;
    if (str == "flee") return AiBehavior::Flee;
    if (str == "attack") return AiBehavior::Attack;
    if (str == "custom") return AiBehavior::Custom;
    return AiBehavior::Idle;
}

ItemType SceneParser::parse_item_type(const std::string& str) {
    if (str == "consumable") return ItemType::Consumable;
    if (str == "equipment") return ItemType::Equipment;
    if (str == "weapon") return ItemType::Weapon;
    if (str == "key") return ItemType::Key;
    if (str == "quest") return ItemType::Quest;
    if (str == "currency") return ItemType::Currency;
    return ItemType::Misc;
}

ItemRarity SceneParser::parse_item_rarity(const std::string& str) {
    if (str == "uncommon") return ItemRarity::Uncommon;
    if (str == "rare") return ItemRarity::Rare;
    if (str == "epic") return ItemRarity::Epic;
    if (str == "legendary") return ItemRarity::Legendary;
    return ItemRarity::Common;
}

StatusEffectType SceneParser::parse_status_effect_type(const std::string& str) {
    if (str == "debuff") return StatusEffectType::Debuff;
    if (str == "dot") return StatusEffectType::Dot;
    if (str == "hot") return StatusEffectType::Hot;
    if (str == "crowd_control") return StatusEffectType::Crowd_Control;
    return StatusEffectType::Buff;
}

ObjectiveType SceneParser::parse_objective_type(const std::string& str) {
    if (str == "kill") return ObjectiveType::Kill;
    if (str == "collect") return ObjectiveType::Collect;
    if (str == "talk") return ObjectiveType::Talk;
    if (str == "reach") return ObjectiveType::Reach;
    if (str == "escort") return ObjectiveType::Escort;
    if (str == "defend") return ObjectiveType::Defend;
    return ObjectiveType::Custom;
}

PickingMode SceneParser::parse_picking_mode(const std::string& str) {
    if (str == "none") return PickingMode::None;
    if (str == "hover") return PickingMode::Hover;
    if (str == "both") return PickingMode::Both;
    return PickingMode::Click;
}

SpatialType SceneParser::parse_spatial_type(const std::string& str) {
    if (str == "none") return SpatialType::None;
    if (str == "octree") return SpatialType::Octree;
    if (str == "grid") return SpatialType::Grid;
    return SpatialType::BVH;
}

TextureFilter SceneParser::parse_texture_filter(const std::string& str) {
    if (str == "nearest") return TextureFilter::Nearest;
    if (str == "trilinear") return TextureFilter::Trilinear;
    if (str == "anisotropic") return TextureFilter::Anisotropic;
    return TextureFilter::Linear;
}

TextureWrap SceneParser::parse_texture_wrap(const std::string& str) {
    if (str == "clamp") return TextureWrap::Clamp;
    if (str == "mirror") return TextureWrap::Mirror;
    if (str == "border") return TextureWrap::Border;
    return TextureWrap::Repeat;
}

} // namespace void_runtime
