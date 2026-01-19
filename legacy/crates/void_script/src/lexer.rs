//! Lexer/Tokenizer for VoidScript
//!
//! Converts source code into a stream of tokens.

use crate::ScriptError;

/// Token kinds
#[derive(Debug, Clone, PartialEq)]
pub enum TokenKind {
    // Literals
    Int(i64),
    Float(f64),
    String(String),
    Bool(bool),
    Null,

    // Identifiers and keywords
    Ident(String),
    Let,
    Fn,
    Return,
    If,
    Else,
    While,
    For,
    In,
    Break,
    Continue,
    True,
    False,

    // Operators
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Eq,
    EqEq,
    BangEq,
    Lt,
    LtEq,
    Gt,
    GtEq,
    And,
    Or,
    Bang,

    // Delimiters
    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Comma,
    Semicolon,
    Colon,
    Dot,
    Arrow,

    // Special
    Eof,
}

/// A token with position information
#[derive(Debug, Clone)]
pub struct Token {
    pub kind: TokenKind,
    pub line: usize,
    pub column: usize,
}

impl Token {
    pub fn new(kind: TokenKind, line: usize, column: usize) -> Self {
        Self { kind, line, column }
    }
}

/// Lexer for VoidScript
pub struct Lexer<'a> {
    source: &'a str,
    chars: std::iter::Peekable<std::str::CharIndices<'a>>,
    line: usize,
    column: usize,
    current_pos: usize,
}

impl<'a> Lexer<'a> {
    /// Create a new lexer for the given source
    pub fn new(source: &'a str) -> Self {
        Self {
            source,
            chars: source.char_indices().peekable(),
            line: 1,
            column: 1,
            current_pos: 0,
        }
    }

    /// Tokenize the entire source
    pub fn tokenize(mut self) -> Result<Vec<Token>, ScriptError> {
        let mut tokens = Vec::new();

        loop {
            let token = self.next_token()?;
            let is_eof = token.kind == TokenKind::Eof;
            tokens.push(token);
            if is_eof {
                break;
            }
        }

        Ok(tokens)
    }

    /// Get the next token
    fn next_token(&mut self) -> Result<Token, ScriptError> {
        self.skip_whitespace_and_comments();

        let (line, column) = (self.line, self.column);

        let Some((pos, c)) = self.advance() else {
            return Ok(Token::new(TokenKind::Eof, line, column));
        };

        let kind = match c {
            // Single-character tokens
            '(' => TokenKind::LParen,
            ')' => TokenKind::RParen,
            '{' => TokenKind::LBrace,
            '}' => TokenKind::RBrace,
            '[' => TokenKind::LBracket,
            ']' => TokenKind::RBracket,
            ',' => TokenKind::Comma,
            ';' => TokenKind::Semicolon,
            ':' => TokenKind::Colon,
            '.' => TokenKind::Dot,
            '+' => TokenKind::Plus,
            '*' => TokenKind::Star,
            '/' => TokenKind::Slash,
            '%' => TokenKind::Percent,

            // Two-character tokens
            '-' => {
                if self.match_char('>') {
                    TokenKind::Arrow
                } else {
                    TokenKind::Minus
                }
            }
            '=' => {
                if self.match_char('=') {
                    TokenKind::EqEq
                } else {
                    TokenKind::Eq
                }
            }
            '!' => {
                if self.match_char('=') {
                    TokenKind::BangEq
                } else {
                    TokenKind::Bang
                }
            }
            '<' => {
                if self.match_char('=') {
                    TokenKind::LtEq
                } else {
                    TokenKind::Lt
                }
            }
            '>' => {
                if self.match_char('=') {
                    TokenKind::GtEq
                } else {
                    TokenKind::Gt
                }
            }
            '&' => {
                if self.match_char('&') {
                    TokenKind::And
                } else {
                    return Err(ScriptError::LexerError(
                        format!("Unexpected character '&' at {}:{}", line, column)
                    ));
                }
            }
            '|' => {
                if self.match_char('|') {
                    TokenKind::Or
                } else {
                    return Err(ScriptError::LexerError(
                        format!("Unexpected character '|' at {}:{}", line, column)
                    ));
                }
            }

            // String literals
            '"' => self.string()?,

            // Numbers
            c if c.is_ascii_digit() => self.number(pos)?,

            // Identifiers and keywords
            c if c.is_alphabetic() || c == '_' => self.identifier(pos),

            _ => {
                return Err(ScriptError::LexerError(
                    format!("Unexpected character '{}' at {}:{}", c, line, column)
                ));
            }
        };

        Ok(Token::new(kind, line, column))
    }

    /// Advance to the next character
    fn advance(&mut self) -> Option<(usize, char)> {
        let result = self.chars.next();
        if let Some((pos, c)) = result {
            self.current_pos = pos;
            if c == '\n' {
                self.line += 1;
                self.column = 1;
            } else {
                self.column += 1;
            }
        }
        result
    }

    /// Peek at the next character
    fn peek(&mut self) -> Option<char> {
        self.chars.peek().map(|(_, c)| *c)
    }

    /// Match and consume a specific character
    fn match_char(&mut self, expected: char) -> bool {
        if self.peek() == Some(expected) {
            self.advance();
            true
        } else {
            false
        }
    }

    /// Skip whitespace and comments
    fn skip_whitespace_and_comments(&mut self) {
        loop {
            match self.peek() {
                Some(' ' | '\t' | '\r' | '\n') => {
                    self.advance();
                }
                Some('/') => {
                    // Check for comment
                    let mut chars_clone = self.chars.clone();
                    chars_clone.next();
                    if chars_clone.peek().map(|(_, c)| *c) == Some('/') {
                        // Line comment
                        self.advance(); // consume first /
                        self.advance(); // consume second /
                        while let Some(c) = self.peek() {
                            if c == '\n' {
                                break;
                            }
                            self.advance();
                        }
                    } else {
                        break;
                    }
                }
                _ => break,
            }
        }
    }

    /// Parse a string literal
    fn string(&mut self) -> Result<TokenKind, ScriptError> {
        let mut value = String::new();
        let start_line = self.line;
        let start_column = self.column;

        loop {
            match self.advance() {
                Some((_, '"')) => break,
                Some((_, '\\')) => {
                    match self.advance() {
                        Some((_, 'n')) => value.push('\n'),
                        Some((_, 't')) => value.push('\t'),
                        Some((_, 'r')) => value.push('\r'),
                        Some((_, '\\')) => value.push('\\'),
                        Some((_, '"')) => value.push('"'),
                        Some((_, c)) => {
                            value.push('\\');
                            value.push(c);
                        }
                        None => {
                            return Err(ScriptError::LexerError(
                                format!("Unterminated string starting at {}:{}", start_line, start_column)
                            ));
                        }
                    }
                }
                Some((_, c)) => value.push(c),
                None => {
                    return Err(ScriptError::LexerError(
                        format!("Unterminated string starting at {}:{}", start_line, start_column)
                    ));
                }
            }
        }

        Ok(TokenKind::String(value))
    }

    /// Parse a number
    fn number(&mut self, start: usize) -> Result<TokenKind, ScriptError> {
        while let Some(c) = self.peek() {
            if c.is_ascii_digit() {
                self.advance();
            } else {
                break;
            }
        }

        // Check for float
        if self.peek() == Some('.') {
            // Look ahead to see if it's a float or method call
            let mut chars_clone = self.chars.clone();
            chars_clone.next();
            if chars_clone.peek().map(|(_, c)| c.is_ascii_digit()).unwrap_or(false) {
                self.advance(); // consume the dot
                while let Some(c) = self.peek() {
                    if c.is_ascii_digit() {
                        self.advance();
                    } else {
                        break;
                    }
                }

                let end = self.chars.peek()
                    .map(|(pos, _)| *pos)
                    .unwrap_or(self.source.len());

                let num_str = &self.source[start..end];
                let value: f64 = num_str.parse()
                    .map_err(|_| ScriptError::LexerError(format!("Invalid number: {}", num_str)))?;

                return Ok(TokenKind::Float(value));
            }
        }

        let end = self.chars.peek()
            .map(|(pos, _)| *pos)
            .unwrap_or(self.source.len());

        let num_str = &self.source[start..end];
        let value: i64 = num_str.parse()
            .map_err(|_| ScriptError::LexerError(format!("Invalid number: {}", num_str)))?;

        Ok(TokenKind::Int(value))
    }

    /// Parse an identifier or keyword
    fn identifier(&mut self, start: usize) -> TokenKind {
        while let Some(c) = self.peek() {
            if c.is_alphanumeric() || c == '_' {
                self.advance();
            } else {
                break;
            }
        }

        let end = self.chars.peek()
            .map(|(pos, _)| *pos)
            .unwrap_or(self.source.len());

        let ident = &self.source[start..end];

        // Check for keywords
        match ident {
            "let" => TokenKind::Let,
            "fn" => TokenKind::Fn,
            "return" => TokenKind::Return,
            "if" => TokenKind::If,
            "else" => TokenKind::Else,
            "while" => TokenKind::While,
            "for" => TokenKind::For,
            "in" => TokenKind::In,
            "break" => TokenKind::Break,
            "continue" => TokenKind::Continue,
            "true" => TokenKind::Bool(true),
            "false" => TokenKind::Bool(false),
            "null" => TokenKind::Null,
            _ => TokenKind::Ident(ident.to_string()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_simple_tokens() {
        let tokens = Lexer::new("+ - * /").tokenize().unwrap();
        assert!(matches!(tokens[0].kind, TokenKind::Plus));
        assert!(matches!(tokens[1].kind, TokenKind::Minus));
        assert!(matches!(tokens[2].kind, TokenKind::Star));
        assert!(matches!(tokens[3].kind, TokenKind::Slash));
    }

    #[test]
    fn test_numbers() {
        let tokens = Lexer::new("42 3.14").tokenize().unwrap();
        assert!(matches!(tokens[0].kind, TokenKind::Int(42)));
        assert!(matches!(tokens[1].kind, TokenKind::Float(f) if (f - 3.14).abs() < 0.001));
    }

    #[test]
    fn test_strings() {
        let tokens = Lexer::new(r#""hello world""#).tokenize().unwrap();
        assert!(matches!(&tokens[0].kind, TokenKind::String(s) if s == "hello world"));
    }

    #[test]
    fn test_keywords() {
        let tokens = Lexer::new("let fn if else while").tokenize().unwrap();
        assert!(matches!(tokens[0].kind, TokenKind::Let));
        assert!(matches!(tokens[1].kind, TokenKind::Fn));
        assert!(matches!(tokens[2].kind, TokenKind::If));
        assert!(matches!(tokens[3].kind, TokenKind::Else));
        assert!(matches!(tokens[4].kind, TokenKind::While));
    }

    #[test]
    fn test_comparison() {
        let tokens = Lexer::new("== != < <= > >=").tokenize().unwrap();
        assert!(matches!(tokens[0].kind, TokenKind::EqEq));
        assert!(matches!(tokens[1].kind, TokenKind::BangEq));
        assert!(matches!(tokens[2].kind, TokenKind::Lt));
        assert!(matches!(tokens[3].kind, TokenKind::LtEq));
        assert!(matches!(tokens[4].kind, TokenKind::Gt));
        assert!(matches!(tokens[5].kind, TokenKind::GtEq));
    }

    #[test]
    fn test_comments() {
        let tokens = Lexer::new("1 // comment\n2").tokenize().unwrap();
        assert!(matches!(tokens[0].kind, TokenKind::Int(1)));
        assert!(matches!(tokens[1].kind, TokenKind::Int(2)));
    }
}
