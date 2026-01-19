//! String built-in functions for VoidScript
//!
//! Provides functions for string manipulation:
//! - upper, lower: Case conversion
//! - trim, trim_start, trim_end: Whitespace removal
//! - split, join: String splitting and joining
//! - contains, starts_with, ends_with: Substring checks
//! - replace: String replacement
//! - substr: Substring extraction
//! - pad_left, pad_right: String padding
//! - repeat: String repetition
//! - char_at, char_code: Character access
//! - format: String formatting

use crate::interpreter::Interpreter;
use crate::value::Value;

/// Register string functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_upper(interpreter);
    register_lower(interpreter);
    register_trim(interpreter);
    register_trim_start(interpreter);
    register_trim_end(interpreter);
    register_split(interpreter);
    register_join(interpreter);
    register_contains(interpreter);
    register_starts_with(interpreter);
    register_ends_with(interpreter);
    register_replace(interpreter);
    register_replace_all(interpreter);
    register_substr(interpreter);
    register_substring(interpreter);
    register_pad_left(interpreter);
    register_pad_right(interpreter);
    register_repeat(interpreter);
    register_char_at(interpreter);
    register_char_code(interpreter);
    register_from_char_code(interpreter);
    register_format(interpreter);
    register_capitalize(interpreter);
    register_title_case(interpreter);
    register_is_empty(interpreter);
    register_is_blank(interpreter);
    register_lines(interpreter);
    register_words(interpreter);
    register_chars(interpreter);
}

/// upper(string) - Convert string to uppercase
fn register_upper(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("upper", 1, |args| {
        match &args[0] {
            Value::String(s) => Ok(Value::String(s.to_uppercase())),
            _ => Err("upper() expects a string".to_string()),
        }
    });
}

/// lower(string) - Convert string to lowercase
fn register_lower(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("lower", 1, |args| {
        match &args[0] {
            Value::String(s) => Ok(Value::String(s.to_lowercase())),
            _ => Err("lower() expects a string".to_string()),
        }
    });
}

/// trim(string) - Remove whitespace from both ends
fn register_trim(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("trim", 1, |args| {
        match &args[0] {
            Value::String(s) => Ok(Value::String(s.trim().to_string())),
            _ => Err("trim() expects a string".to_string()),
        }
    });
}

/// trim_start(string) - Remove whitespace from start
fn register_trim_start(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("trim_start", 1, |args| {
        match &args[0] {
            Value::String(s) => Ok(Value::String(s.trim_start().to_string())),
            _ => Err("trim_start() expects a string".to_string()),
        }
    });
}

/// trim_end(string) - Remove whitespace from end
fn register_trim_end(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("trim_end", 1, |args| {
        match &args[0] {
            Value::String(s) => Ok(Value::String(s.trim_end().to_string())),
            _ => Err("trim_end() expects a string".to_string()),
        }
    });
}

/// split(string, separator) - Split string by separator
fn register_split(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("split", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::String(s), Value::String(sep)) => {
                let parts: Vec<Value> = s.split(sep.as_str())
                    .map(|p| Value::String(p.to_string()))
                    .collect();
                Ok(Value::Array(parts))
            }
            _ => Err("split() expects two strings".to_string()),
        }
    });
}

/// join(array, separator) - Join array elements with separator
fn register_join(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("join", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Array(arr), Value::String(sep)) => {
                let parts: Vec<String> = arr.iter()
                    .map(|v| v.to_string_value())
                    .collect();
                Ok(Value::String(parts.join(sep)))
            }
            _ => Err("join() expects an array and a string".to_string()),
        }
    });
}

/// contains(string, substring) - Check if string contains substring
fn register_contains(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("contains", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::String(s), Value::String(sub)) => {
                Ok(Value::Bool(s.contains(sub.as_str())))
            }
            (Value::Array(arr), val) => {
                Ok(Value::Bool(arr.contains(val)))
            }
            _ => Err("contains() expects a string/array and a value".to_string()),
        }
    });
}

/// starts_with(string, prefix) - Check if string starts with prefix
fn register_starts_with(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("starts_with", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::String(s), Value::String(prefix)) => {
                Ok(Value::Bool(s.starts_with(prefix.as_str())))
            }
            _ => Err("starts_with() expects two strings".to_string()),
        }
    });
}

/// ends_with(string, suffix) - Check if string ends with suffix
fn register_ends_with(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("ends_with", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::String(s), Value::String(suffix)) => {
                Ok(Value::Bool(s.ends_with(suffix.as_str())))
            }
            _ => Err("ends_with() expects two strings".to_string()),
        }
    });
}

/// replace(string, from, to) - Replace first occurrence
fn register_replace(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("replace", 3, |args| {
        match (&args[0], &args[1], &args[2]) {
            (Value::String(s), Value::String(from), Value::String(to)) => {
                Ok(Value::String(s.replacen(from.as_str(), to.as_str(), 1)))
            }
            _ => Err("replace() expects three strings".to_string()),
        }
    });
}

/// replace_all(string, from, to) - Replace all occurrences
fn register_replace_all(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("replace_all", 3, |args| {
        match (&args[0], &args[1], &args[2]) {
            (Value::String(s), Value::String(from), Value::String(to)) => {
                Ok(Value::String(s.replace(from.as_str(), to.as_str())))
            }
            _ => Err("replace_all() expects three strings".to_string()),
        }
    });
}

/// substr(string, start, length) - Extract substring by start position and length
fn register_substr(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("substr", 3, |args| {
        match (&args[0], &args[1], &args[2]) {
            (Value::String(s), Value::Int(start), Value::Int(len)) => {
                let chars: Vec<char> = s.chars().collect();
                let start = if *start < 0 {
                    (chars.len() as i64 + start).max(0) as usize
                } else {
                    *start as usize
                };
                let len = *len as usize;
                let end = (start + len).min(chars.len());
                if start >= chars.len() {
                    Ok(Value::String(String::new()))
                } else {
                    Ok(Value::String(chars[start..end].iter().collect()))
                }
            }
            _ => Err("substr() expects a string and two integers".to_string()),
        }
    });
}

/// substring(string, start, end?) - Extract substring by start and end positions
fn register_substring(interpreter: &mut Interpreter) {
    interpreter.register_native("substring", |args| {
        if args.len() < 2 || args.len() > 3 {
            return Err("substring() expects 2-3 arguments".to_string());
        }

        match &args[0] {
            Value::String(s) => {
                let chars: Vec<char> = s.chars().collect();
                let start = match &args[1] {
                    Value::Int(n) => {
                        if *n < 0 {
                            (chars.len() as i64 + n).max(0) as usize
                        } else {
                            (*n as usize).min(chars.len())
                        }
                    }
                    _ => return Err("substring() indices must be integers".to_string()),
                };

                let end = if args.len() == 3 {
                    match &args[2] {
                        Value::Int(n) => {
                            if *n < 0 {
                                (chars.len() as i64 + n).max(0) as usize
                            } else {
                                (*n as usize).min(chars.len())
                            }
                        }
                        _ => return Err("substring() indices must be integers".to_string()),
                    }
                } else {
                    chars.len()
                };

                if start >= end {
                    Ok(Value::String(String::new()))
                } else {
                    Ok(Value::String(chars[start..end].iter().collect()))
                }
            }
            _ => Err("substring() expects a string".to_string()),
        }
    });
}

/// pad_left(string, length, char?) - Pad string on the left
fn register_pad_left(interpreter: &mut Interpreter) {
    interpreter.register_native("pad_left", |args| {
        if args.len() < 2 || args.len() > 3 {
            return Err("pad_left() expects 2-3 arguments".to_string());
        }

        match (&args[0], &args[1]) {
            (Value::String(s), Value::Int(len)) => {
                let pad_char = if args.len() == 3 {
                    match &args[2] {
                        Value::String(c) => c.chars().next().unwrap_or(' '),
                        _ => return Err("pad_left() pad character must be a string".to_string()),
                    }
                } else {
                    ' '
                };

                let target_len = *len as usize;
                if s.len() >= target_len {
                    Ok(Value::String(s.clone()))
                } else {
                    let padding: String = std::iter::repeat(pad_char)
                        .take(target_len - s.chars().count())
                        .collect();
                    Ok(Value::String(format!("{}{}", padding, s)))
                }
            }
            _ => Err("pad_left() expects a string and integer".to_string()),
        }
    });
}

/// pad_right(string, length, char?) - Pad string on the right
fn register_pad_right(interpreter: &mut Interpreter) {
    interpreter.register_native("pad_right", |args| {
        if args.len() < 2 || args.len() > 3 {
            return Err("pad_right() expects 2-3 arguments".to_string());
        }

        match (&args[0], &args[1]) {
            (Value::String(s), Value::Int(len)) => {
                let pad_char = if args.len() == 3 {
                    match &args[2] {
                        Value::String(c) => c.chars().next().unwrap_or(' '),
                        _ => return Err("pad_right() pad character must be a string".to_string()),
                    }
                } else {
                    ' '
                };

                let target_len = *len as usize;
                if s.chars().count() >= target_len {
                    Ok(Value::String(s.clone()))
                } else {
                    let padding: String = std::iter::repeat(pad_char)
                        .take(target_len - s.chars().count())
                        .collect();
                    Ok(Value::String(format!("{}{}", s, padding)))
                }
            }
            _ => Err("pad_right() expects a string and integer".to_string()),
        }
    });
}

/// repeat(string, count) - Repeat string n times
fn register_repeat(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("repeat", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::String(s), Value::Int(n)) => {
                if *n < 0 {
                    Err("repeat() count must be non-negative".to_string())
                } else {
                    Ok(Value::String(s.repeat(*n as usize)))
                }
            }
            _ => Err("repeat() expects a string and integer".to_string()),
        }
    });
}

/// char_at(string, index) - Get character at index
fn register_char_at(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("char_at", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::String(s), Value::Int(idx)) => {
                let chars: Vec<char> = s.chars().collect();
                let idx = if *idx < 0 {
                    (chars.len() as i64 + idx) as usize
                } else {
                    *idx as usize
                };
                chars.get(idx)
                    .map(|c| Value::String(c.to_string()))
                    .ok_or_else(|| "Index out of bounds".to_string())
            }
            _ => Err("char_at() expects a string and integer".to_string()),
        }
    });
}

/// char_code(string) - Get Unicode code point of first character
fn register_char_code(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("char_code", 1, |args| {
        match &args[0] {
            Value::String(s) => {
                s.chars().next()
                    .map(|c| Value::Int(c as i64))
                    .ok_or_else(|| "String is empty".to_string())
            }
            _ => Err("char_code() expects a string".to_string()),
        }
    });
}

/// from_char_code(code) - Create string from Unicode code point
fn register_from_char_code(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("from_char_code", 1, |args| {
        match &args[0] {
            Value::Int(code) => {
                char::from_u32(*code as u32)
                    .map(|c| Value::String(c.to_string()))
                    .ok_or_else(|| format!("Invalid Unicode code point: {}", code))
            }
            _ => Err("from_char_code() expects an integer".to_string()),
        }
    });
}

/// format(template, ...args) - Format string with placeholders
fn register_format(interpreter: &mut Interpreter) {
    interpreter.register_native("format", |args| {
        if args.is_empty() {
            return Err("format() requires at least one argument".to_string());
        }

        match &args[0] {
            Value::String(template) => {
                let mut result = template.clone();
                for (i, arg) in args.iter().skip(1).enumerate() {
                    let placeholder = format!("{{{}}}", i);
                    result = result.replace(&placeholder, &arg.to_string_value());
                }
                // Also replace {} with sequential arguments
                let mut idx = 0;
                while result.contains("{}") && idx < args.len() - 1 {
                    result = result.replacen("{}", &args[idx + 1].to_string_value(), 1);
                    idx += 1;
                }
                Ok(Value::String(result))
            }
            _ => Err("format() expects a string template".to_string()),
        }
    });
}

/// capitalize(string) - Capitalize first character
fn register_capitalize(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("capitalize", 1, |args| {
        match &args[0] {
            Value::String(s) => {
                let mut chars = s.chars();
                match chars.next() {
                    None => Ok(Value::String(String::new())),
                    Some(c) => {
                        let capitalized: String = c.to_uppercase()
                            .chain(chars)
                            .collect();
                        Ok(Value::String(capitalized))
                    }
                }
            }
            _ => Err("capitalize() expects a string".to_string()),
        }
    });
}

/// title_case(string) - Capitalize first character of each word
fn register_title_case(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("title_case", 1, |args| {
        match &args[0] {
            Value::String(s) => {
                let result: String = s.split_whitespace()
                    .map(|word| {
                        let mut chars = word.chars();
                        match chars.next() {
                            None => String::new(),
                            Some(c) => c.to_uppercase()
                                .chain(chars.flat_map(|c| c.to_lowercase()))
                                .collect()
                        }
                    })
                    .collect::<Vec<_>>()
                    .join(" ");
                Ok(Value::String(result))
            }
            _ => Err("title_case() expects a string".to_string()),
        }
    });
}

/// is_empty(string) - Check if string is empty
fn register_is_empty(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_empty", 1, |args| {
        match &args[0] {
            Value::String(s) => Ok(Value::Bool(s.is_empty())),
            Value::Array(arr) => Ok(Value::Bool(arr.is_empty())),
            Value::Object(obj) => Ok(Value::Bool(obj.is_empty())),
            _ => Err("is_empty() expects a string, array, or object".to_string()),
        }
    });
}

/// is_blank(string) - Check if string is empty or only whitespace
fn register_is_blank(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("is_blank", 1, |args| {
        match &args[0] {
            Value::String(s) => Ok(Value::Bool(s.trim().is_empty())),
            _ => Err("is_blank() expects a string".to_string()),
        }
    });
}

/// lines(string) - Split string into lines
fn register_lines(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("lines", 1, |args| {
        match &args[0] {
            Value::String(s) => {
                let lines: Vec<Value> = s.lines()
                    .map(|line| Value::String(line.to_string()))
                    .collect();
                Ok(Value::Array(lines))
            }
            _ => Err("lines() expects a string".to_string()),
        }
    });
}

/// words(string) - Split string into words
fn register_words(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("words", 1, |args| {
        match &args[0] {
            Value::String(s) => {
                let words: Vec<Value> = s.split_whitespace()
                    .map(|word| Value::String(word.to_string()))
                    .collect();
                Ok(Value::Array(words))
            }
            _ => Err("words() expects a string".to_string()),
        }
    });
}

/// chars(string) - Split string into characters
fn register_chars(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("chars", 1, |args| {
        match &args[0] {
            Value::String(s) => {
                let chars: Vec<Value> = s.chars()
                    .map(|c| Value::String(c.to_string()))
                    .collect();
                Ok(Value::Array(chars))
            }
            _ => Err("chars() expects a string".to_string()),
        }
    });
}

#[cfg(test)]
mod tests {
    use crate::VoidScript;
    use crate::value::Value;

    fn run(code: &str) -> Value {
        let mut vs = VoidScript::new();
        vs.execute(code).unwrap()
    }

    #[test]
    fn test_upper_lower() {
        assert_eq!(run(r#"upper("hello");"#), Value::String("HELLO".to_string()));
        assert_eq!(run(r#"lower("HELLO");"#), Value::String("hello".to_string()));
    }

    #[test]
    fn test_trim() {
        assert_eq!(run(r#"trim("  hello  ");"#), Value::String("hello".to_string()));
        assert_eq!(run(r#"trim_start("  hello");"#), Value::String("hello".to_string()));
        assert_eq!(run(r#"trim_end("hello  ");"#), Value::String("hello".to_string()));
    }

    #[test]
    fn test_split_join() {
        let result = run(r#"split("a,b,c", ",");"#);
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 3);
            assert_eq!(arr[0], Value::String("a".to_string()));
        } else {
            panic!("Expected array");
        }

        assert_eq!(
            run(r#"join(["a", "b", "c"], "-");"#),
            Value::String("a-b-c".to_string())
        );
    }

    #[test]
    fn test_contains() {
        assert_eq!(run(r#"contains("hello world", "world");"#), Value::Bool(true));
        assert_eq!(run(r#"contains("hello world", "xyz");"#), Value::Bool(false));
    }

    #[test]
    fn test_starts_ends_with() {
        assert_eq!(run(r#"starts_with("hello", "hel");"#), Value::Bool(true));
        assert_eq!(run(r#"ends_with("hello", "llo");"#), Value::Bool(true));
    }

    #[test]
    fn test_replace() {
        assert_eq!(
            run(r#"replace("hello world", "world", "rust");"#),
            Value::String("hello rust".to_string())
        );
        assert_eq!(
            run(r#"replace_all("a-b-c", "-", "_");"#),
            Value::String("a_b_c".to_string())
        );
    }

    #[test]
    fn test_substr() {
        assert_eq!(run(r#"substr("hello", 1, 3);"#), Value::String("ell".to_string()));
        assert_eq!(run(r#"substring("hello", 1, 4);"#), Value::String("ell".to_string()));
    }

    #[test]
    fn test_pad() {
        assert_eq!(run(r#"pad_left("42", 5, "0");"#), Value::String("00042".to_string()));
        assert_eq!(run(r#"pad_right("hi", 5);"#), Value::String("hi   ".to_string()));
    }

    #[test]
    fn test_repeat() {
        assert_eq!(run(r#"repeat("ab", 3);"#), Value::String("ababab".to_string()));
    }

    #[test]
    fn test_char_at() {
        assert_eq!(run(r#"char_at("hello", 1);"#), Value::String("e".to_string()));
        assert_eq!(run(r#"char_at("hello", -1);"#), Value::String("o".to_string()));
    }

    #[test]
    fn test_char_code() {
        assert_eq!(run(r#"char_code("A");"#), Value::Int(65));
        assert_eq!(run(r#"from_char_code(65);"#), Value::String("A".to_string()));
    }

    #[test]
    fn test_format() {
        assert_eq!(
            run(r#"format("Hello, {}!", "world");"#),
            Value::String("Hello, world!".to_string())
        );
        assert_eq!(
            run(r#"format("{0} + {1} = {2}", 1, 2, 3);"#),
            Value::String("1 + 2 = 3".to_string())
        );
    }

    #[test]
    fn test_capitalize() {
        assert_eq!(run(r#"capitalize("hello");"#), Value::String("Hello".to_string()));
        assert_eq!(run(r#"title_case("hello world");"#), Value::String("Hello World".to_string()));
    }

    #[test]
    fn test_is_empty_blank() {
        assert_eq!(run(r#"is_empty("");"#), Value::Bool(true));
        assert_eq!(run(r#"is_empty("a");"#), Value::Bool(false));
        assert_eq!(run(r#"is_blank("  ");"#), Value::Bool(true));
        assert_eq!(run(r#"is_blank(" a ");"#), Value::Bool(false));
    }

    #[test]
    fn test_lines_words_chars() {
        let result = run(r#"lines("a\nb\nc");"#);
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 3);
        } else {
            panic!("Expected array");
        }

        let result = run(r#"words("hello world");"#);
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 2);
        } else {
            panic!("Expected array");
        }

        let result = run(r#"chars("abc");"#);
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 3);
        } else {
            panic!("Expected array");
        }
    }
}
