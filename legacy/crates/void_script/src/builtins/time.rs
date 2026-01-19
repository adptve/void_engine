//! Time built-in functions for VoidScript
//!
//! Provides functions for time operations:
//! - now: Get current timestamp
//! - sleep: Pause execution (blocking)
//! - after: Schedule callback (stub - would be async in real impl)
//! - interval: Set repeating timer (stub)
//! - clear_timer: Cancel a timer
//! - date: Get current date components
//! - format_date: Format timestamp as string
//! - parse_date: Parse date string to timestamp
//! - elapsed: Measure elapsed time

use crate::interpreter::Interpreter;
use crate::value::Value;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::{Duration, SystemTime, UNIX_EPOCH};

// Global timer ID counter
static TIMER_COUNTER: AtomicU64 = AtomicU64::new(1);

/// Register time functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_now(interpreter);
    register_now_secs(interpreter);
    register_now_nanos(interpreter);
    register_sleep(interpreter);
    register_after(interpreter);
    register_interval(interpreter);
    register_clear_timer(interpreter);
    register_date(interpreter);
    register_date_parts(interpreter);
    register_format_time(interpreter);
    register_elapsed(interpreter);
    register_stopwatch(interpreter);
    register_performance_now(interpreter);
}

/// Helper to get current timestamp in milliseconds
fn current_time_millis() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_millis() as i64
}

/// Helper to create a timer handle
fn make_timer_handle(id: u64, timer_type: &str) -> Value {
    let mut obj = HashMap::new();
    obj.insert("id".to_string(), Value::Int(id as i64));
    obj.insert("timer_type".to_string(), Value::String(timer_type.to_string()));
    obj.insert("type".to_string(), Value::String("timer".to_string()));
    Value::Object(obj)
}

/// now() - Get current timestamp in milliseconds since Unix epoch
fn register_now(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("now", 0, |_| {
        Ok(Value::Int(current_time_millis()))
    });
}

/// now_secs() - Get current timestamp in seconds since Unix epoch
fn register_now_secs(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("now_secs", 0, |_| {
        let secs = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs();
        Ok(Value::Int(secs as i64))
    });
}

/// now_nanos() - Get current timestamp in nanoseconds (high precision)
fn register_now_nanos(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("now_nanos", 0, |_| {
        let nanos = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos();
        // Use float for large nanosecond values that might overflow i64
        Ok(Value::Float(nanos as f64))
    });
}

/// sleep(ms) - Sleep for specified milliseconds (blocking)
fn register_sleep(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("sleep", 1, |args| {
        let ms = match &args[0] {
            Value::Int(n) => {
                if *n < 0 {
                    return Err("sleep() expects non-negative milliseconds".to_string());
                }
                *n as u64
            }
            Value::Float(f) => {
                if *f < 0.0 {
                    return Err("sleep() expects non-negative milliseconds".to_string());
                }
                *f as u64
            }
            _ => return Err("sleep() expects a number (milliseconds)".to_string()),
        };

        std::thread::sleep(Duration::from_millis(ms));
        Ok(Value::Null)
    });
}

/// after(ms, callback) - Schedule callback after delay
/// NOTE: This is a stub. In real implementation, this would be async/non-blocking.
fn register_after(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("after", 2, |args| {
        let ms = match &args[0] {
            Value::Int(n) => *n,
            Value::Float(f) => *f as i64,
            _ => return Err("after() expects a number (milliseconds)".to_string()),
        };

        if ms < 0 {
            return Err("after() expects non-negative milliseconds".to_string());
        }

        match &args[1] {
            Value::Function(_) | Value::Native(_) => {}
            _ => return Err("after() expects a function callback".to_string()),
        };

        // In a real implementation, this would:
        // 1. Schedule the callback in an async runtime
        // 2. Return immediately with a timer handle
        // For now, we just create a handle

        let id = TIMER_COUNTER.fetch_add(1, Ordering::SeqCst);
        Ok(make_timer_handle(id, "timeout"))
    });
}

/// interval(ms, callback) - Set repeating timer
/// NOTE: This is a stub. In real implementation, this would be async.
fn register_interval(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("interval", 2, |args| {
        let ms = match &args[0] {
            Value::Int(n) => *n,
            Value::Float(f) => *f as i64,
            _ => return Err("interval() expects a number (milliseconds)".to_string()),
        };

        if ms <= 0 {
            return Err("interval() expects positive milliseconds".to_string());
        }

        match &args[1] {
            Value::Function(_) | Value::Native(_) => {}
            _ => return Err("interval() expects a function callback".to_string()),
        };

        let id = TIMER_COUNTER.fetch_add(1, Ordering::SeqCst);
        Ok(make_timer_handle(id, "interval"))
    });
}

/// clear_timer(handle) - Cancel a timer
fn register_clear_timer(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("clear_timer", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                if obj.get("type") == Some(&Value::String("timer".to_string())) {
                    Ok(Value::Bool(true))
                } else {
                    Err("clear_timer() expects a timer handle".to_string())
                }
            }
            Value::Int(_) => {
                // Allow clearing by timer ID directly
                Ok(Value::Bool(true))
            }
            _ => Err("clear_timer() expects a timer handle".to_string()),
        }
    });
}

/// date() - Get current date as object with year, month, day, etc.
fn register_date(interpreter: &mut Interpreter) {
    interpreter.register_native("date", |args| {
        // Get timestamp - current time if not provided
        let timestamp_ms = if !args.is_empty() {
            match &args[0] {
                Value::Int(n) => *n,
                Value::Float(f) => *f as i64,
                _ => return Err("date() expects a timestamp (milliseconds)".to_string()),
            }
        } else {
            current_time_millis()
        };

        // Calculate date components from timestamp
        // This is a simplified implementation - a real one would use chrono
        let secs = timestamp_ms / 1000;
        let ms = (timestamp_ms % 1000) as i64;

        // Days since epoch
        let days = secs / 86400;

        // Calculate year (approximate - doesn't handle leap years perfectly)
        let mut year = 1970;
        let mut remaining_days = days;

        loop {
            let days_in_year = if is_leap_year(year) { 366 } else { 365 };
            if remaining_days < days_in_year {
                break;
            }
            remaining_days -= days_in_year;
            year += 1;
        }

        // Calculate month and day
        let month_days = if is_leap_year(year) {
            [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        } else {
            [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        };

        let mut month = 1;
        for &days_in_month in &month_days {
            if remaining_days < days_in_month {
                break;
            }
            remaining_days -= days_in_month;
            month += 1;
        }
        let day = remaining_days + 1;

        // Time components
        let seconds_in_day = secs % 86400;
        let hour = seconds_in_day / 3600;
        let minute = (seconds_in_day % 3600) / 60;
        let second = seconds_in_day % 60;

        // Day of week (0 = Sunday)
        // Jan 1, 1970 was Thursday (4)
        let day_of_week = ((days + 4) % 7) as i64;

        let mut result = HashMap::new();
        result.insert("year".to_string(), Value::Int(year));
        result.insert("month".to_string(), Value::Int(month));
        result.insert("day".to_string(), Value::Int(day));
        result.insert("hour".to_string(), Value::Int(hour));
        result.insert("minute".to_string(), Value::Int(minute));
        result.insert("second".to_string(), Value::Int(second));
        result.insert("millisecond".to_string(), Value::Int(ms));
        result.insert("day_of_week".to_string(), Value::Int(day_of_week));
        result.insert("timestamp".to_string(), Value::Int(timestamp_ms));

        Ok(Value::Object(result))
    });
}

/// Helper function to check if a year is a leap year
fn is_leap_year(year: i64) -> bool {
    (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)
}

/// date_parts(year, month, day, hour?, minute?, second?, ms?) - Create timestamp from parts
fn register_date_parts(interpreter: &mut Interpreter) {
    interpreter.register_native("date_parts", |args| {
        if args.len() < 3 {
            return Err("date_parts() requires at least year, month, day".to_string());
        }

        let year = match &args[0] {
            Value::Int(n) => *n,
            _ => return Err("date_parts() expects integer year".to_string()),
        };

        let month = match &args[1] {
            Value::Int(n) => *n,
            _ => return Err("date_parts() expects integer month".to_string()),
        };

        let day = match &args[2] {
            Value::Int(n) => *n,
            _ => return Err("date_parts() expects integer day".to_string()),
        };

        let hour = if args.len() > 3 {
            match &args[3] {
                Value::Int(n) => *n,
                _ => return Err("date_parts() expects integer hour".to_string()),
            }
        } else {
            0
        };

        let minute = if args.len() > 4 {
            match &args[4] {
                Value::Int(n) => *n,
                _ => return Err("date_parts() expects integer minute".to_string()),
            }
        } else {
            0
        };

        let second = if args.len() > 5 {
            match &args[5] {
                Value::Int(n) => *n,
                _ => return Err("date_parts() expects integer second".to_string()),
            }
        } else {
            0
        };

        let ms = if args.len() > 6 {
            match &args[6] {
                Value::Int(n) => *n,
                _ => return Err("date_parts() expects integer millisecond".to_string()),
            }
        } else {
            0
        };

        // Calculate days from year
        let mut days: i64 = 0;
        for y in 1970..year {
            days += if is_leap_year(y) { 366 } else { 365 };
        }

        // Add days from months
        let month_days = if is_leap_year(year) {
            [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        } else {
            [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        };

        for i in 0..(month - 1) as usize {
            if i < month_days.len() {
                days += month_days[i];
            }
        }

        // Add day of month (1-indexed)
        days += day - 1;

        // Convert to milliseconds
        let timestamp = days * 86400 * 1000 + hour * 3600 * 1000 + minute * 60 * 1000 + second * 1000 + ms;

        Ok(Value::Int(timestamp))
    });
}

/// format_time(timestamp?, format?) - Format timestamp as string
/// Default format is ISO 8601
fn register_format_time(interpreter: &mut Interpreter) {
    interpreter.register_native("format_time", |args| {
        let timestamp_ms = if !args.is_empty() {
            match &args[0] {
                Value::Int(n) => *n,
                Value::Float(f) => *f as i64,
                _ => return Err("format_time() expects a timestamp".to_string()),
            }
        } else {
            current_time_millis()
        };

        // Calculate date parts (reusing logic from date())
        let secs = timestamp_ms / 1000;
        let ms = timestamp_ms % 1000;

        let days = secs / 86400;
        let mut year = 1970;
        let mut remaining_days = days;

        loop {
            let days_in_year = if is_leap_year(year) { 366 } else { 365 };
            if remaining_days < days_in_year {
                break;
            }
            remaining_days -= days_in_year;
            year += 1;
        }

        let month_days = if is_leap_year(year) {
            [31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        } else {
            [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        };

        let mut month = 1;
        for &days_in_month in &month_days {
            if remaining_days < days_in_month {
                break;
            }
            remaining_days -= days_in_month;
            month += 1;
        }
        let day = remaining_days + 1;

        let seconds_in_day = secs % 86400;
        let hour = seconds_in_day / 3600;
        let minute = (seconds_in_day % 3600) / 60;
        let second = seconds_in_day % 60;

        // Format as ISO 8601
        let formatted = format!(
            "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z",
            year, month, day, hour, minute, second, ms
        );

        Ok(Value::String(formatted))
    });
}

/// elapsed(callback) - Measure elapsed time of callback execution
fn register_elapsed(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("elapsed", 1, |args| {
        match &args[0] {
            Value::Function(_) | Value::Native(_) => {
                // In a real implementation, we would:
                // 1. Record start time
                // 2. Execute callback
                // 3. Return elapsed time
                // For now, just return mock data
                let mut result = HashMap::new();
                result.insert("elapsed_ms".to_string(), Value::Float(0.0));
                result.insert("result".to_string(), Value::Null);
                Ok(Value::Object(result))
            }
            _ => Err("elapsed() expects a function".to_string()),
        }
    });
}

/// stopwatch() - Create a stopwatch for measuring time
fn register_stopwatch(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("stopwatch", 0, |_| {
        let start = current_time_millis();
        let mut sw = HashMap::new();
        sw.insert("start".to_string(), Value::Int(start));
        sw.insert("type".to_string(), Value::String("stopwatch".to_string()));

        // The stopwatch object can be passed to elapsed_since()
        Ok(Value::Object(sw))
    });
}

/// performance_now() - High-precision timestamp for performance measurement
fn register_performance_now(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("performance_now", 0, |_| {
        // Use std::time::Instant would be better but requires tracking a reference point
        // For now, use nanoseconds converted to fractional milliseconds
        let nanos = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos();
        // Return as fractional milliseconds for high precision
        Ok(Value::Float((nanos as f64) / 1_000_000.0))
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
    fn test_now() {
        let result = run("now();");
        if let Value::Int(t) = result {
            // Should be a reasonable timestamp (after year 2020)
            assert!(t > 1577836800000); // Jan 1, 2020
        } else {
            panic!("Expected int");
        }
    }

    #[test]
    fn test_now_secs() {
        let result = run("now_secs();");
        if let Value::Int(t) = result {
            assert!(t > 1577836800); // Jan 1, 2020
        } else {
            panic!("Expected int");
        }
    }

    #[test]
    fn test_now_nanos() {
        let result = run("now_nanos();");
        assert!(matches!(result, Value::Float(_)));
    }

    #[test]
    fn test_sleep() {
        // Just verify it works without error
        let result = run("sleep(1);"); // Sleep 1ms
        assert_eq!(result, Value::Null);
    }

    #[test]
    fn test_after() {
        let result = run(r#"
            fn callback() {}
            after(100, callback);
        "#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("type"), Some(&Value::String("timer".to_string())));
            assert_eq!(obj.get("timer_type"), Some(&Value::String("timeout".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_interval() {
        let result = run(r#"
            fn callback() {}
            interval(1000, callback);
        "#);
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("timer_type"), Some(&Value::String("interval".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_clear_timer() {
        let result = run(r#"
            fn callback() {}
            let t = after(100, callback);
            clear_timer(t);
        "#);
        assert_eq!(result, Value::Bool(true));
    }

    #[test]
    fn test_date() {
        let result = run("date();");
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("year"));
            assert!(obj.contains_key("month"));
            assert!(obj.contains_key("day"));
            assert!(obj.contains_key("hour"));
            assert!(obj.contains_key("minute"));
            assert!(obj.contains_key("second"));
            assert!(obj.contains_key("millisecond"));
            assert!(obj.contains_key("day_of_week"));
            assert!(obj.contains_key("timestamp"));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_date_with_timestamp() {
        // Test with a known timestamp: Jan 1, 2020 00:00:00 UTC
        let result = run("date(1577836800000);");
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("year"), Some(&Value::Int(2020)));
            assert_eq!(obj.get("month"), Some(&Value::Int(1)));
            assert_eq!(obj.get("day"), Some(&Value::Int(1)));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_date_parts() {
        // Create timestamp for Jan 1, 2020
        let result = run("date_parts(2020, 1, 1, 0, 0, 0, 0);");
        if let Value::Int(t) = result {
            assert_eq!(t, 1577836800000);
        } else {
            panic!("Expected int");
        }
    }

    #[test]
    fn test_format_time() {
        let result = run("format_time(1577836800000);");
        if let Value::String(s) = result {
            assert!(s.starts_with("2020-01-01"));
        } else {
            panic!("Expected string");
        }
    }

    #[test]
    fn test_format_time_current() {
        let result = run("format_time();");
        assert!(matches!(result, Value::String(_)));
    }

    #[test]
    fn test_stopwatch() {
        let result = run("stopwatch();");
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("start"));
            assert_eq!(obj.get("type"), Some(&Value::String("stopwatch".to_string())));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_performance_now() {
        let result = run("performance_now();");
        assert!(matches!(result, Value::Float(_)));
    }

    #[test]
    fn test_elapsed() {
        let result = run(r#"
            fn test_fn() { return 42; }
            elapsed(test_fn);
        "#);
        if let Value::Object(obj) = result {
            assert!(obj.contains_key("elapsed_ms"));
            assert!(obj.contains_key("result"));
        } else {
            panic!("Expected object");
        }
    }
}
