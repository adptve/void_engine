#include "lexer.hpp"

#include <cctype>
#include <charconv>

namespace void_script {

// =============================================================================
// Keyword Map
// =============================================================================

const std::unordered_map<std::string_view, TokenType> Lexer::keywords_ = {
    {"let", TokenType::Let},
    {"const", TokenType::Const},
    {"var", TokenType::Var},
    {"fn", TokenType::Fn},
    {"return", TokenType::Return},
    {"if", TokenType::If},
    {"else", TokenType::Else},
    {"while", TokenType::While},
    {"for", TokenType::For},
    {"in", TokenType::In},
    {"break", TokenType::Break},
    {"continue", TokenType::Continue},
    {"match", TokenType::Match},
    {"class", TokenType::Class},
    {"struct", TokenType::Struct},
    {"enum", TokenType::Enum},
    {"this", TokenType::This},
    {"super", TokenType::Super},
    {"new", TokenType::New},
    {"import", TokenType::Import},
    {"export", TokenType::Export},
    {"from", TokenType::From},
    {"as", TokenType::As},
    {"module", TokenType::Module},
    {"pub", TokenType::Pub},
    {"try", TokenType::Try},
    {"catch", TokenType::Catch},
    {"finally", TokenType::Finally},
    {"throw", TokenType::Throw},
    {"async", TokenType::Async},
    {"await", TokenType::Await},
    {"yield", TokenType::Yield},
    {"type", TokenType::Type},
    {"interface", TokenType::Interface},
    {"impl", TokenType::Impl},
    {"static", TokenType::Static},
    {"true", TokenType::True},
    {"false", TokenType::False},
    {"null", TokenType::Null},
};

// =============================================================================
// Lexer Implementation
// =============================================================================

Lexer::Lexer(std::string_view source, std::string_view filename)
    : source_(source), filename_(filename) {}

Token Lexer::next_token() {
    if (peeked_token_) {
        Token tok = std::move(*peeked_token_);
        peeked_token_.reset();
        return tok;
    }

    skip_whitespace();

    start_ = current_;

    if (is_at_end()) {
        return make_token(TokenType::Eof);
    }

    char c = advance();

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return scan_identifier();
    }

    // Numbers
    if (std::isdigit(c)) {
        return scan_number();
    }

    // Operators and punctuation
    switch (c) {
        case '(': return make_token(TokenType::LeftParen);
        case ')': return make_token(TokenType::RightParen);
        case '{': return make_token(TokenType::LeftBrace);
        case '}': return make_token(TokenType::RightBrace);
        case '[': return make_token(TokenType::LeftBracket);
        case ']': return make_token(TokenType::RightBracket);
        case ',': return make_token(TokenType::Comma);
        case ';': return make_token(TokenType::Semicolon);
        case '~': return make_token(TokenType::Tilde);
        case '@': return make_token(TokenType::At);
        case '#': return make_token(TokenType::Hash);
        case '\\': return make_token(TokenType::Backslash);

        case '.':
            if (match('.')) {
                if (match('.')) return make_token(TokenType::DotDotDot);
                return make_token(TokenType::DotDot);
            }
            return make_token(TokenType::Dot);

        case ':':
            if (match(':')) return make_token(TokenType::ColonColon);
            return make_token(TokenType::Colon);

        case '+':
            if (match('+')) return make_token(TokenType::Increment);
            if (match('=')) return make_token(TokenType::PlusAssign);
            return make_token(TokenType::Plus);

        case '-':
            if (match('-')) return make_token(TokenType::Decrement);
            if (match('=')) return make_token(TokenType::MinusAssign);
            if (match('>')) return make_token(TokenType::Arrow);
            return make_token(TokenType::Minus);

        case '*':
            if (match('*')) return make_token(TokenType::Power);
            if (match('=')) return make_token(TokenType::StarAssign);
            return make_token(TokenType::Star);

        case '/':
            if (match('/')) {
                skip_line_comment();
                return next_token();
            }
            if (match('*')) {
                skip_block_comment();
                return next_token();
            }
            if (match('=')) return make_token(TokenType::SlashAssign);
            return make_token(TokenType::Slash);

        case '%':
            if (match('=')) return make_token(TokenType::PercentAssign);
            return make_token(TokenType::Percent);

        case '&':
            if (match('&')) return make_token(TokenType::And);
            if (match('=')) return make_token(TokenType::AmpersandAssign);
            return make_token(TokenType::Ampersand);

        case '|':
            if (match('|')) return make_token(TokenType::Or);
            if (match('=')) return make_token(TokenType::PipeAssign);
            return make_token(TokenType::Pipe);

        case '^':
            if (match('=')) return make_token(TokenType::CaretAssign);
            return make_token(TokenType::Caret);

        case '=':
            if (match('=')) return make_token(TokenType::Equal);
            if (match('>')) return make_token(TokenType::FatArrow);
            return make_token(TokenType::Assign);

        case '!':
            if (match('=')) return make_token(TokenType::NotEqual);
            return make_token(TokenType::Not);

        case '<':
            if (match('<')) {
                if (match('=')) return make_token(TokenType::ShiftLeftAssign);
                return make_token(TokenType::ShiftLeft);
            }
            if (match('=')) {
                if (match('>')) return make_token(TokenType::Spaceship);
                return make_token(TokenType::LessEqual);
            }
            return make_token(TokenType::Less);

        case '>':
            if (match('>')) {
                if (match('=')) return make_token(TokenType::ShiftRightAssign);
                return make_token(TokenType::ShiftRight);
            }
            if (match('=')) return make_token(TokenType::GreaterEqual);
            return make_token(TokenType::Greater);

        case '?':
            if (match('?')) return make_token(TokenType::QuestionQuestion);
            if (match('.')) return make_token(TokenType::QuestionDot);
            return make_token(TokenType::Question);

        case '"': return scan_string('"');
        case '\'': return scan_string('\'');

        default:
            return error_token("Unexpected character");
    }
}

Token Lexer::peek_token() {
    if (!peeked_token_) {
        peeked_token_ = next_token();
    }
    return *peeked_token_;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::Eof) break;
    }
    return tokens;
}

SourceLocation Lexer::location() const {
    return {filename_, line_, static_cast<std::uint32_t>(column_ - (current_ - start_)), static_cast<std::uint32_t>(start_)};
}

char Lexer::peek() const {
    if (is_at_end()) return '\0';
    return source_[current_];
}

char Lexer::peek_next() const {
    if (current_ + 1 >= source_.size()) return '\0';
    return source_[current_ + 1];
}

char Lexer::advance() {
    char c = source_[current_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (is_at_end()) return false;
    if (source_[current_] != expected) return false;
    advance();
    return true;
}

void Lexer::skip_whitespace() {
    while (!is_at_end()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;
            case '\n':
                advance();
                break;
            default:
                return;
        }
    }
}

void Lexer::skip_line_comment() {
    while (!is_at_end() && peek() != '\n') {
        advance();
    }
}

void Lexer::skip_block_comment() {
    int nesting = 1;
    while (!is_at_end() && nesting > 0) {
        if (peek() == '/' && peek_next() == '*') {
            advance();
            advance();
            ++nesting;
        } else if (peek() == '*' && peek_next() == '/') {
            advance();
            advance();
            --nesting;
        } else {
            advance();
        }
    }

    if (nesting > 0) {
        errors_.emplace_back(ScriptError::UnterminatedComment,
                             "Unterminated block comment", location());
    }
}

Token Lexer::make_token(TokenType type) {
    Token tok;
    tok.type = type;
    tok.lexeme = source_.substr(start_, current_ - start_);
    tok.location = location();
    return tok;
}

Token Lexer::error_token(const std::string& message) {
    errors_.emplace_back(ScriptError::UnexpectedCharacter, message, location());

    Token tok;
    tok.type = TokenType::Error;
    tok.lexeme = source_.substr(start_, current_ - start_);
    tok.location = location();
    tok.string_value = message;
    return tok;
}

Token Lexer::scan_identifier() {
    while (std::isalnum(peek()) || peek() == '_') {
        advance();
    }

    std::string_view text = source_.substr(start_, current_ - start_);
    auto it = keywords_.find(text);

    TokenType type = (it != keywords_.end()) ? it->second : TokenType::Identifier;
    return make_token(type);
}

Token Lexer::scan_number() {
    bool is_float = false;
    bool is_hex = false;
    bool is_binary = false;

    // Check for hex or binary
    if (peek() == 'x' || peek() == 'X') {
        advance();
        is_hex = true;
        while (std::isxdigit(peek())) {
            advance();
        }
    } else if (peek() == 'b' || peek() == 'B') {
        advance();
        is_binary = true;
        while (peek() == '0' || peek() == '1') {
            advance();
        }
    } else {
        // Decimal
        while (std::isdigit(peek())) {
            advance();
        }

        // Fractional part
        if (peek() == '.' && std::isdigit(peek_next())) {
            is_float = true;
            advance();  // .
            while (std::isdigit(peek())) {
                advance();
            }
        }

        // Exponent
        if (peek() == 'e' || peek() == 'E') {
            is_float = true;
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            while (std::isdigit(peek())) {
                advance();
            }
        }
    }

    Token tok = make_token(is_float ? TokenType::Float : TokenType::Integer);

    // Parse value
    std::string_view text = tok.lexeme;
    if (is_float) {
        auto result = std::from_chars(text.data(), text.data() + text.size(), tok.float_value);
        if (result.ec != std::errc{}) {
            return error_token("Invalid number");
        }
    } else if (is_hex) {
        text = text.substr(2);  // Skip 0x
        auto result = std::from_chars(text.data(), text.data() + text.size(), tok.int_value, 16);
        if (result.ec != std::errc{}) {
            return error_token("Invalid hex number");
        }
    } else if (is_binary) {
        text = text.substr(2);  // Skip 0b
        auto result = std::from_chars(text.data(), text.data() + text.size(), tok.int_value, 2);
        if (result.ec != std::errc{}) {
            return error_token("Invalid binary number");
        }
    } else {
        auto result = std::from_chars(text.data(), text.data() + text.size(), tok.int_value);
        if (result.ec != std::errc{}) {
            return error_token("Invalid number");
        }
    }

    return tok;
}

Token Lexer::scan_string(char quote) {
    std::string value;
    bool escape = false;

    while (!is_at_end()) {
        char c = peek();

        if (escape) {
            escape = false;
            switch (c) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case '0': value += '\0'; break;
                default:
                    errors_.emplace_back(ScriptError::InvalidEscape,
                                        "Invalid escape sequence", location());
                    value += c;
                    break;
            }
            advance();
        } else if (c == '\\') {
            escape = true;
            advance();
        } else if (c == quote) {
            advance();
            Token tok = make_token(TokenType::String);
            tok.string_value = std::move(value);
            return tok;
        } else if (c == '\n') {
            return error_token("Unterminated string");
        } else {
            value += c;
            advance();
        }
    }

    return error_token("Unterminated string");
}

} // namespace void_script
