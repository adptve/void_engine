//! Command parser
//!
//! Parses command line input into Command structures.

use crate::Command;

/// Parse error
#[derive(Debug, thiserror::Error)]
pub enum ParseError {
    #[error("Empty input")]
    EmptyInput,

    #[error("Unexpected token: {0}")]
    UnexpectedToken(String),

    #[error("Unclosed quote")]
    UnclosedQuote,

    #[error("Invalid escape sequence")]
    InvalidEscape,

    #[error("Syntax error: {0}")]
    SyntaxError(String),
}

/// Token type
#[derive(Debug, Clone, PartialEq)]
pub enum Token {
    /// Word (command name or argument)
    Word(String),
    /// Long option (--name)
    LongOption(String),
    /// Short option (-x)
    ShortOption(char),
    /// Option value (=value after option)
    OptionValue(String),
    /// Pipe operator (|)
    Pipe,
    /// Background operator (&)
    Background,
    /// Semicolon (;)
    Semicolon,
    /// Redirect output (>)
    RedirectOut,
    /// Redirect append (>>)
    RedirectAppend,
    /// Redirect input (<)
    RedirectIn,
}

/// Command parser
pub struct Parser {
    /// Whether to allow shell operators (|, &, etc.)
    allow_operators: bool,
}

impl Parser {
    /// Create a new parser
    pub fn new() -> Self {
        Self {
            allow_operators: true,
        }
    }

    /// Create a simple parser (no operators)
    pub fn simple() -> Self {
        Self {
            allow_operators: false,
        }
    }

    /// Parse a command line
    pub fn parse(&self, input: &str) -> Result<Command, ParseError> {
        let input = input.trim();
        if input.is_empty() {
            return Err(ParseError::EmptyInput);
        }

        let tokens = self.tokenize(input)?;
        self.parse_tokens(&tokens)
    }

    /// Tokenize input string
    fn tokenize(&self, input: &str) -> Result<Vec<Token>, ParseError> {
        let mut tokens = Vec::new();
        let mut chars = input.chars().peekable();

        while let Some(&c) = chars.peek() {
            match c {
                // Whitespace - skip
                ' ' | '\t' => {
                    chars.next();
                }

                // Double-quoted string
                '"' => {
                    chars.next();
                    let mut word = String::new();
                    let mut closed = false;

                    while let Some(&c) = chars.peek() {
                        chars.next();
                        match c {
                            '"' => {
                                closed = true;
                                break;
                            }
                            '\\' => {
                                if let Some(&next) = chars.peek() {
                                    chars.next();
                                    match next {
                                        'n' => word.push('\n'),
                                        't' => word.push('\t'),
                                        'r' => word.push('\r'),
                                        '\\' => word.push('\\'),
                                        '"' => word.push('"'),
                                        _ => {
                                            word.push('\\');
                                            word.push(next);
                                        }
                                    }
                                }
                            }
                            _ => word.push(c),
                        }
                    }

                    if !closed {
                        return Err(ParseError::UnclosedQuote);
                    }

                    tokens.push(Token::Word(word));
                }

                // Single-quoted string (no escapes)
                '\'' => {
                    chars.next();
                    let mut word = String::new();
                    let mut closed = false;

                    while let Some(&c) = chars.peek() {
                        chars.next();
                        if c == '\'' {
                            closed = true;
                            break;
                        }
                        word.push(c);
                    }

                    if !closed {
                        return Err(ParseError::UnclosedQuote);
                    }

                    tokens.push(Token::Word(word));
                }

                // Long option
                '-' if chars.clone().nth(1) == Some('-') => {
                    chars.next(); // first -
                    chars.next(); // second -

                    let mut name = String::new();
                    while let Some(&c) = chars.peek() {
                        if c.is_alphanumeric() || c == '-' || c == '_' {
                            name.push(c);
                            chars.next();
                        } else {
                            break;
                        }
                    }

                    if name.is_empty() {
                        return Err(ParseError::SyntaxError("Empty option name".to_string()));
                    }

                    tokens.push(Token::LongOption(name));

                    // Check for =value
                    if chars.peek() == Some(&'=') {
                        chars.next();
                        let mut value = String::new();
                        while let Some(&c) = chars.peek() {
                            if c == ' ' || c == '\t' {
                                break;
                            }
                            value.push(c);
                            chars.next();
                        }
                        tokens.push(Token::OptionValue(value));
                    }
                }

                // Short option
                '-' => {
                    chars.next();
                    while let Some(&c) = chars.peek() {
                        if c.is_alphabetic() {
                            tokens.push(Token::ShortOption(c));
                            chars.next();
                        } else {
                            break;
                        }
                    }
                }

                // Operators
                '|' if self.allow_operators => {
                    chars.next();
                    tokens.push(Token::Pipe);
                }

                '&' if self.allow_operators => {
                    chars.next();
                    tokens.push(Token::Background);
                }

                ';' if self.allow_operators => {
                    chars.next();
                    tokens.push(Token::Semicolon);
                }

                '>' if self.allow_operators => {
                    chars.next();
                    if chars.peek() == Some(&'>') {
                        chars.next();
                        tokens.push(Token::RedirectAppend);
                    } else {
                        tokens.push(Token::RedirectOut);
                    }
                }

                '<' if self.allow_operators => {
                    chars.next();
                    tokens.push(Token::RedirectIn);
                }

                // Regular word
                _ => {
                    let mut word = String::new();
                    while let Some(&c) = chars.peek() {
                        if c == ' ' || c == '\t' || c == '"' || c == '\''
                            || (self.allow_operators && matches!(c, '|' | '&' | ';' | '>' | '<'))
                        {
                            break;
                        }
                        word.push(c);
                        chars.next();
                    }
                    if !word.is_empty() {
                        tokens.push(Token::Word(word));
                    }
                }
            }
        }

        Ok(tokens)
    }

    /// Parse tokens into command
    fn parse_tokens(&self, tokens: &[Token]) -> Result<Command, ParseError> {
        if tokens.is_empty() {
            return Err(ParseError::EmptyInput);
        }

        let mut iter = tokens.iter().peekable();

        // First token should be the command name
        let name = match iter.next() {
            Some(Token::Word(w)) => w.clone(),
            Some(t) => return Err(ParseError::UnexpectedToken(format!("{:?}", t))),
            None => return Err(ParseError::EmptyInput),
        };

        let mut cmd = Command::new(name);
        let mut pending_option: Option<String> = None;

        while let Some(token) = iter.next() {
            match token {
                Token::Word(w) => {
                    if let Some(opt) = pending_option.take() {
                        cmd.options.insert(opt, Some(w.clone()));
                    } else {
                        cmd.args.push(w.clone());
                    }
                }

                Token::LongOption(name) => {
                    // If we have a pending option, it was a flag
                    if let Some(opt) = pending_option.take() {
                        cmd.options.insert(opt, None);
                    }
                    pending_option = Some(name.clone());
                }

                Token::OptionValue(value) => {
                    if let Some(opt) = pending_option.take() {
                        cmd.options.insert(opt, Some(value.clone()));
                    }
                }

                Token::ShortOption(c) => {
                    // If we have a pending option, it was a flag
                    if let Some(opt) = pending_option.take() {
                        cmd.options.insert(opt, None);
                    }
                    cmd.short_options.push(*c);
                }

                Token::Pipe => {
                    // Finish any pending option
                    if let Some(opt) = pending_option.take() {
                        cmd.options.insert(opt, None);
                    }

                    // Parse the rest as a piped command
                    let remaining: Vec<_> = iter.cloned().collect();
                    if !remaining.is_empty() {
                        let piped = self.parse_tokens(&remaining)?;
                        cmd.pipe_to = Some(Box::new(piped));
                    }
                    break;
                }

                Token::Background => {
                    // Finish any pending option
                    if let Some(opt) = pending_option.take() {
                        cmd.options.insert(opt, None);
                    }
                    cmd.background = true;
                }

                Token::Semicolon => {
                    // Just finish this command (multiple commands not supported yet)
                    break;
                }

                Token::RedirectOut | Token::RedirectAppend | Token::RedirectIn => {
                    // Redirects not implemented yet
                }
            }
        }

        // Handle any remaining pending option
        if let Some(opt) = pending_option {
            cmd.options.insert(opt, None);
        }

        Ok(cmd)
    }
}

impl Default for Parser {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_simple() {
        let parser = Parser::new();
        let cmd = parser.parse("help").unwrap();
        assert_eq!(cmd.name, "help");
        assert!(cmd.args.is_empty());
    }

    #[test]
    fn test_parse_with_args() {
        let parser = Parser::new();
        let cmd = parser.parse("spawn my_app layer1").unwrap();
        assert_eq!(cmd.name, "spawn");
        assert_eq!(cmd.args, vec!["my_app", "layer1"]);
    }

    #[test]
    fn test_parse_quoted() {
        let parser = Parser::new();
        let cmd = parser.parse(r#"echo "hello world""#).unwrap();
        assert_eq!(cmd.name, "echo");
        assert_eq!(cmd.args, vec!["hello world"]);
    }

    #[test]
    fn test_parse_options() {
        let parser = Parser::new();
        let cmd = parser.parse("spawn --layer 1 --verbose").unwrap();
        assert_eq!(cmd.name, "spawn");
        assert_eq!(cmd.get_option("layer"), Some("1"));
        assert!(cmd.has_option("verbose"));
    }

    #[test]
    fn test_parse_option_equals() {
        let parser = Parser::new();
        let cmd = parser.parse("spawn --layer=1").unwrap();
        assert_eq!(cmd.get_option("layer"), Some("1"));
    }

    #[test]
    fn test_parse_short_options() {
        let parser = Parser::new();
        let cmd = parser.parse("cmd -abc").unwrap();
        assert!(cmd.has_short('a'));
        assert!(cmd.has_short('b'));
        assert!(cmd.has_short('c'));
    }

    #[test]
    fn test_parse_pipe() {
        let parser = Parser::new();
        let cmd = parser.parse("list apps | filter active").unwrap();
        assert_eq!(cmd.name, "list");
        assert!(cmd.pipe_to.is_some());

        let piped = cmd.pipe_to.as_ref().unwrap();
        assert_eq!(piped.name, "filter");
    }

    #[test]
    fn test_parse_background() {
        let parser = Parser::new();
        let cmd = parser.parse("long_task &").unwrap();
        assert!(cmd.background);
    }

    #[test]
    fn test_unclosed_quote() {
        let parser = Parser::new();
        let result = parser.parse(r#"echo "unclosed"#);
        assert!(matches!(result, Err(ParseError::UnclosedQuote)));
    }

    #[test]
    fn test_escape_sequences() {
        let parser = Parser::new();
        let cmd = parser.parse(r#"echo "line1\nline2""#).unwrap();
        assert_eq!(cmd.args[0], "line1\nline2");
    }
}
