//! Math built-in functions for VoidScript
//!
//! Provides mathematical functions:
//! - abs, sign: Absolute value and sign
//! - min, max: Minimum and maximum
//! - floor, ceil, round, trunc: Rounding
//! - sqrt, cbrt: Roots
//! - pow, exp, log, log10, log2: Exponential and logarithmic
//! - sin, cos, tan, asin, acos, atan, atan2: Trigonometric
//! - sinh, cosh, tanh: Hyperbolic
//! - random: Random number generation
//! - clamp, lerp: Value manipulation
//! - deg_to_rad, rad_to_deg: Angle conversion

use crate::interpreter::Interpreter;
use crate::value::Value;

/// Register math functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    // Basic math
    register_abs(interpreter);
    register_sign(interpreter);
    register_min(interpreter);
    register_max(interpreter);

    // Rounding
    register_floor(interpreter);
    register_ceil(interpreter);
    register_round(interpreter);
    register_trunc(interpreter);

    // Roots
    register_sqrt(interpreter);
    register_cbrt(interpreter);

    // Exponential and logarithmic
    register_pow(interpreter);
    register_exp(interpreter);
    register_log(interpreter);
    register_log10(interpreter);
    register_log2(interpreter);

    // Trigonometric
    register_sin(interpreter);
    register_cos(interpreter);
    register_tan(interpreter);
    register_asin(interpreter);
    register_acos(interpreter);
    register_atan(interpreter);
    register_atan2(interpreter);

    // Hyperbolic
    register_sinh(interpreter);
    register_cosh(interpreter);
    register_tanh(interpreter);

    // Random
    register_random(interpreter);
    register_random_int(interpreter);
    register_random_range(interpreter);

    // Value manipulation
    register_clamp(interpreter);
    register_lerp(interpreter);
    register_map_range(interpreter);

    // Angle conversion
    register_deg_to_rad(interpreter);
    register_rad_to_deg(interpreter);

    // Additional math
    register_hypot(interpreter);
    register_fract(interpreter);
    register_mod(interpreter);
    register_gcd(interpreter);
    register_lcm(interpreter);

    // Constants
    interpreter.set_var("PI", Value::Float(std::f64::consts::PI));
    interpreter.set_var("E", Value::Float(std::f64::consts::E));
    interpreter.set_var("TAU", Value::Float(std::f64::consts::TAU));
    interpreter.set_var("INFINITY", Value::Float(f64::INFINITY));
    interpreter.set_var("NEG_INFINITY", Value::Float(f64::NEG_INFINITY));
    interpreter.set_var("NAN", Value::Float(f64::NAN));
}

/// abs(x) - Absolute value
fn register_abs(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("abs", 1, |args| {
        match &args[0] {
            Value::Int(n) => Ok(Value::Int(n.abs())),
            Value::Float(f) => Ok(Value::Float(f.abs())),
            _ => Err("abs() expects a number".to_string()),
        }
    });
}

/// sign(x) - Return -1, 0, or 1 based on sign
fn register_sign(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("sign", 1, |args| {
        match &args[0] {
            Value::Int(n) => Ok(Value::Int(n.signum())),
            Value::Float(f) => {
                if f.is_nan() {
                    Ok(Value::Float(f64::NAN))
                } else if *f == 0.0 {
                    Ok(Value::Int(0))
                } else if *f > 0.0 {
                    Ok(Value::Int(1))
                } else {
                    Ok(Value::Int(-1))
                }
            }
            _ => Err("sign() expects a number".to_string()),
        }
    });
}

/// min(...args) - Minimum of arguments
fn register_min(interpreter: &mut Interpreter) {
    interpreter.register_native("min", |args| {
        if args.is_empty() {
            return Err("min() requires at least one argument".to_string());
        }

        // Handle array argument
        let values = if args.len() == 1 {
            match &args[0] {
                Value::Array(arr) => arr.clone(),
                _ => args.clone(),
            }
        } else {
            args.clone()
        };

        if values.is_empty() {
            return Err("min() requires non-empty input".to_string());
        }

        let mut min_val = values[0].clone();
        for arg in values.iter().skip(1) {
            let (min_f, arg_f) = match (&min_val, arg) {
                (Value::Int(a), Value::Int(b)) => {
                    if *b < *a {
                        min_val = arg.clone();
                    }
                    continue;
                }
                (Value::Float(a), Value::Float(b)) => (*a, *b),
                (Value::Int(a), Value::Float(b)) => (*a as f64, *b),
                (Value::Float(a), Value::Int(b)) => (*a, *b as f64),
                _ => return Err("min() expects numbers".to_string()),
            };

            if arg_f < min_f {
                min_val = arg.clone();
            }
        }

        Ok(min_val)
    });
}

/// max(...args) - Maximum of arguments
fn register_max(interpreter: &mut Interpreter) {
    interpreter.register_native("max", |args| {
        if args.is_empty() {
            return Err("max() requires at least one argument".to_string());
        }

        // Handle array argument
        let values = if args.len() == 1 {
            match &args[0] {
                Value::Array(arr) => arr.clone(),
                _ => args.clone(),
            }
        } else {
            args.clone()
        };

        if values.is_empty() {
            return Err("max() requires non-empty input".to_string());
        }

        let mut max_val = values[0].clone();
        for arg in values.iter().skip(1) {
            let (max_f, arg_f) = match (&max_val, arg) {
                (Value::Int(a), Value::Int(b)) => {
                    if *b > *a {
                        max_val = arg.clone();
                    }
                    continue;
                }
                (Value::Float(a), Value::Float(b)) => (*a, *b),
                (Value::Int(a), Value::Float(b)) => (*a as f64, *b),
                (Value::Float(a), Value::Int(b)) => (*a, *b as f64),
                _ => return Err("max() expects numbers".to_string()),
            };

            if arg_f > max_f {
                max_val = arg.clone();
            }
        }

        Ok(max_val)
    });
}

/// floor(x) - Round down to nearest integer
fn register_floor(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("floor", 1, |args| {
        match &args[0] {
            Value::Float(f) => Ok(Value::Int(f.floor() as i64)),
            Value::Int(n) => Ok(Value::Int(*n)),
            _ => Err("floor() expects a number".to_string()),
        }
    });
}

/// ceil(x) - Round up to nearest integer
fn register_ceil(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("ceil", 1, |args| {
        match &args[0] {
            Value::Float(f) => Ok(Value::Int(f.ceil() as i64)),
            Value::Int(n) => Ok(Value::Int(*n)),
            _ => Err("ceil() expects a number".to_string()),
        }
    });
}

/// round(x) - Round to nearest integer
fn register_round(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("round", 1, |args| {
        match &args[0] {
            Value::Float(f) => Ok(Value::Int(f.round() as i64)),
            Value::Int(n) => Ok(Value::Int(*n)),
            _ => Err("round() expects a number".to_string()),
        }
    });
}

/// trunc(x) - Truncate toward zero
fn register_trunc(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("trunc", 1, |args| {
        match &args[0] {
            Value::Float(f) => Ok(Value::Int(f.trunc() as i64)),
            Value::Int(n) => Ok(Value::Int(*n)),
            _ => Err("trunc() expects a number".to_string()),
        }
    });
}

/// sqrt(x) - Square root
fn register_sqrt(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("sqrt", 1, |args| {
        match args[0].to_float() {
            Some(f) => {
                if f < 0.0 {
                    Err("sqrt() domain error: negative input".to_string())
                } else {
                    Ok(Value::Float(f.sqrt()))
                }
            }
            None => Err("sqrt() expects a number".to_string()),
        }
    });
}

/// cbrt(x) - Cube root
fn register_cbrt(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("cbrt", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.cbrt())),
            None => Err("cbrt() expects a number".to_string()),
        }
    });
}

/// pow(base, exp) - Power
fn register_pow(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("pow", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Int(base), Value::Int(exp)) => {
                if *exp >= 0 {
                    Ok(Value::Int(base.pow(*exp as u32)))
                } else {
                    Ok(Value::Float((*base as f64).powi(*exp as i32)))
                }
            }
            (Value::Float(base), Value::Int(exp)) => {
                Ok(Value::Float(base.powi(*exp as i32)))
            }
            (Value::Float(base), Value::Float(exp)) => {
                Ok(Value::Float(base.powf(*exp)))
            }
            (Value::Int(base), Value::Float(exp)) => {
                Ok(Value::Float((*base as f64).powf(*exp)))
            }
            _ => Err("pow() expects numbers".to_string()),
        }
    });
}

/// exp(x) - e^x
fn register_exp(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("exp", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.exp())),
            None => Err("exp() expects a number".to_string()),
        }
    });
}

/// log(x) - Natural logarithm
fn register_log(interpreter: &mut Interpreter) {
    interpreter.register_native("log", |args| {
        if args.is_empty() || args.len() > 2 {
            return Err("log() expects 1-2 arguments".to_string());
        }

        let x = match args[0].to_float() {
            Some(f) => f,
            None => return Err("log() expects a number".to_string()),
        };

        if x <= 0.0 {
            return Err("log() domain error: non-positive input".to_string());
        }

        if args.len() == 2 {
            // log(x, base)
            let base = match args[1].to_float() {
                Some(f) => f,
                None => return Err("log() expects a number".to_string()),
            };
            if base <= 0.0 || base == 1.0 {
                return Err("log() domain error: invalid base".to_string());
            }
            Ok(Value::Float(x.log(base)))
        } else {
            // Natural log
            Ok(Value::Float(x.ln()))
        }
    });
}

/// log10(x) - Base 10 logarithm
fn register_log10(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("log10", 1, |args| {
        match args[0].to_float() {
            Some(f) => {
                if f <= 0.0 {
                    Err("log10() domain error: non-positive input".to_string())
                } else {
                    Ok(Value::Float(f.log10()))
                }
            }
            None => Err("log10() expects a number".to_string()),
        }
    });
}

/// log2(x) - Base 2 logarithm
fn register_log2(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("log2", 1, |args| {
        match args[0].to_float() {
            Some(f) => {
                if f <= 0.0 {
                    Err("log2() domain error: non-positive input".to_string())
                } else {
                    Ok(Value::Float(f.log2()))
                }
            }
            None => Err("log2() expects a number".to_string()),
        }
    });
}

/// sin(x) - Sine (radians)
fn register_sin(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("sin", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.sin())),
            None => Err("sin() expects a number".to_string()),
        }
    });
}

/// cos(x) - Cosine (radians)
fn register_cos(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("cos", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.cos())),
            None => Err("cos() expects a number".to_string()),
        }
    });
}

/// tan(x) - Tangent (radians)
fn register_tan(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("tan", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.tan())),
            None => Err("tan() expects a number".to_string()),
        }
    });
}

/// asin(x) - Arc sine
fn register_asin(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("asin", 1, |args| {
        match args[0].to_float() {
            Some(f) => {
                if f < -1.0 || f > 1.0 {
                    Err("asin() domain error: input must be in [-1, 1]".to_string())
                } else {
                    Ok(Value::Float(f.asin()))
                }
            }
            None => Err("asin() expects a number".to_string()),
        }
    });
}

/// acos(x) - Arc cosine
fn register_acos(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("acos", 1, |args| {
        match args[0].to_float() {
            Some(f) => {
                if f < -1.0 || f > 1.0 {
                    Err("acos() domain error: input must be in [-1, 1]".to_string())
                } else {
                    Ok(Value::Float(f.acos()))
                }
            }
            None => Err("acos() expects a number".to_string()),
        }
    });
}

/// atan(x) - Arc tangent
fn register_atan(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("atan", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.atan())),
            None => Err("atan() expects a number".to_string()),
        }
    });
}

/// atan2(y, x) - Two-argument arc tangent
fn register_atan2(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("atan2", 2, |args| {
        match (args[0].to_float(), args[1].to_float()) {
            (Some(y), Some(x)) => Ok(Value::Float(y.atan2(x))),
            _ => Err("atan2() expects two numbers".to_string()),
        }
    });
}

/// sinh(x) - Hyperbolic sine
fn register_sinh(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("sinh", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.sinh())),
            None => Err("sinh() expects a number".to_string()),
        }
    });
}

/// cosh(x) - Hyperbolic cosine
fn register_cosh(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("cosh", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.cosh())),
            None => Err("cosh() expects a number".to_string()),
        }
    });
}

/// tanh(x) - Hyperbolic tangent
fn register_tanh(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("tanh", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.tanh())),
            None => Err("tanh() expects a number".to_string()),
        }
    });
}

/// random() - Random float in [0, 1)
fn register_random(interpreter: &mut Interpreter) {
    use std::time::{SystemTime, UNIX_EPOCH};

    interpreter.register_native_with_arity("random", 0, |_| {
        // Simple LCG random number generator using system time as seed
        let seed = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_nanos() as u64;

        // LCG parameters (same as glibc)
        let a: u64 = 1103515245;
        let c: u64 = 12345;
        let m: u64 = 1 << 31;

        let random_bits = (a.wrapping_mul(seed).wrapping_add(c)) % m;
        let random_float = (random_bits as f64) / (m as f64);

        Ok(Value::Float(random_float))
    });
}

/// random_int(min, max) - Random integer in [min, max]
fn register_random_int(interpreter: &mut Interpreter) {
    use std::time::{SystemTime, UNIX_EPOCH};

    interpreter.register_native_with_arity("random_int", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Int(min), Value::Int(max)) => {
                if min > max {
                    return Err("random_int() min must be <= max".to_string());
                }

                let seed = SystemTime::now()
                    .duration_since(UNIX_EPOCH)
                    .unwrap_or_default()
                    .as_nanos() as u64;

                let a: u64 = 1103515245;
                let c: u64 = 12345;
                let m: u64 = 1 << 31;

                let random_bits = (a.wrapping_mul(seed).wrapping_add(c)) % m;
                let range = (max - min + 1) as u64;
                let random_int = min + (random_bits % range) as i64;

                Ok(Value::Int(random_int))
            }
            _ => Err("random_int() expects two integers".to_string()),
        }
    });
}

/// random_range(min, max) - Random float in [min, max)
fn register_random_range(interpreter: &mut Interpreter) {
    use std::time::{SystemTime, UNIX_EPOCH};

    interpreter.register_native_with_arity("random_range", 2, |args| {
        match (args[0].to_float(), args[1].to_float()) {
            (Some(min), Some(max)) => {
                if min > max {
                    return Err("random_range() min must be <= max".to_string());
                }

                let seed = SystemTime::now()
                    .duration_since(UNIX_EPOCH)
                    .unwrap_or_default()
                    .as_nanos() as u64;

                let a: u64 = 1103515245;
                let c: u64 = 12345;
                let m: u64 = 1 << 31;

                let random_bits = (a.wrapping_mul(seed).wrapping_add(c)) % m;
                let random_float = (random_bits as f64) / (m as f64);
                let result = min + random_float * (max - min);

                Ok(Value::Float(result))
            }
            _ => Err("random_range() expects two numbers".to_string()),
        }
    });
}

/// clamp(value, min, max) - Clamp value to range
fn register_clamp(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("clamp", 3, |args| {
        match (args[0].to_float(), args[1].to_float(), args[2].to_float()) {
            (Some(val), Some(min), Some(max)) => {
                if min > max {
                    return Err("clamp() min must be <= max".to_string());
                }
                Ok(Value::Float(val.max(min).min(max)))
            }
            _ => Err("clamp() expects three numbers".to_string()),
        }
    });
}

/// lerp(a, b, t) - Linear interpolation
fn register_lerp(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("lerp", 3, |args| {
        match (args[0].to_float(), args[1].to_float(), args[2].to_float()) {
            (Some(a), Some(b), Some(t)) => {
                Ok(Value::Float(a + (b - a) * t))
            }
            _ => Err("lerp() expects three numbers".to_string()),
        }
    });
}

/// map_range(value, from_min, from_max, to_min, to_max) - Map value from one range to another
fn register_map_range(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("map_range", 5, |args| {
        let vals: Vec<Option<f64>> = args.iter().map(|a| a.to_float()).collect();
        if vals.iter().any(|v| v.is_none()) {
            return Err("map_range() expects five numbers".to_string());
        }

        let vals: Vec<f64> = vals.into_iter().map(|v| v.unwrap()).collect();
        let (value, from_min, from_max, to_min, to_max) = (vals[0], vals[1], vals[2], vals[3], vals[4]);

        if (from_max - from_min).abs() < f64::EPSILON {
            return Err("map_range() from_min and from_max cannot be equal".to_string());
        }

        let t = (value - from_min) / (from_max - from_min);
        let result = to_min + (to_max - to_min) * t;

        Ok(Value::Float(result))
    });
}

/// deg_to_rad(degrees) - Convert degrees to radians
fn register_deg_to_rad(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("deg_to_rad", 1, |args| {
        match args[0].to_float() {
            Some(deg) => Ok(Value::Float(deg.to_radians())),
            None => Err("deg_to_rad() expects a number".to_string()),
        }
    });
}

/// rad_to_deg(radians) - Convert radians to degrees
fn register_rad_to_deg(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("rad_to_deg", 1, |args| {
        match args[0].to_float() {
            Some(rad) => Ok(Value::Float(rad.to_degrees())),
            None => Err("rad_to_deg() expects a number".to_string()),
        }
    });
}

/// hypot(x, y) - Hypotenuse (sqrt(x*x + y*y))
fn register_hypot(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("hypot", 2, |args| {
        match (args[0].to_float(), args[1].to_float()) {
            (Some(x), Some(y)) => Ok(Value::Float(x.hypot(y))),
            _ => Err("hypot() expects two numbers".to_string()),
        }
    });
}

/// fract(x) - Fractional part of float
fn register_fract(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("fract", 1, |args| {
        match args[0].to_float() {
            Some(f) => Ok(Value::Float(f.fract())),
            None => Err("fract() expects a number".to_string()),
        }
    });
}

/// mod(a, b) - Modulo operation (handles negative numbers correctly)
fn register_mod(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("mod", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Int(a), Value::Int(b)) => {
                if *b == 0 {
                    Err("mod() division by zero".to_string())
                } else {
                    Ok(Value::Int(a.rem_euclid(*b)))
                }
            }
            (Value::Float(a), Value::Float(b)) => {
                if *b == 0.0 {
                    Err("mod() division by zero".to_string())
                } else {
                    Ok(Value::Float(a.rem_euclid(*b)))
                }
            }
            (Value::Int(a), Value::Float(b)) => {
                if *b == 0.0 {
                    Err("mod() division by zero".to_string())
                } else {
                    Ok(Value::Float((*a as f64).rem_euclid(*b)))
                }
            }
            (Value::Float(a), Value::Int(b)) => {
                if *b == 0 {
                    Err("mod() division by zero".to_string())
                } else {
                    Ok(Value::Float(a.rem_euclid(*b as f64)))
                }
            }
            _ => Err("mod() expects two numbers".to_string()),
        }
    });
}

/// gcd(a, b) - Greatest common divisor
fn register_gcd(interpreter: &mut Interpreter) {
    fn gcd(a: i64, b: i64) -> i64 {
        if b == 0 { a.abs() } else { gcd(b, a % b) }
    }

    interpreter.register_native_with_arity("gcd", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Int(a), Value::Int(b)) => Ok(Value::Int(gcd(*a, *b))),
            _ => Err("gcd() expects two integers".to_string()),
        }
    });
}

/// lcm(a, b) - Least common multiple
fn register_lcm(interpreter: &mut Interpreter) {
    fn gcd(a: i64, b: i64) -> i64 {
        if b == 0 { a.abs() } else { gcd(b, a % b) }
    }

    interpreter.register_native_with_arity("lcm", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Int(a), Value::Int(b)) => {
                if *a == 0 || *b == 0 {
                    Ok(Value::Int(0))
                } else {
                    Ok(Value::Int((a.abs() / gcd(*a, *b)) * b.abs()))
                }
            }
            _ => Err("lcm() expects two integers".to_string()),
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

    fn approx_eq(a: f64, b: f64) -> bool {
        (a - b).abs() < 1e-10
    }

    #[test]
    fn test_abs_sign() {
        assert_eq!(run("abs(-5);"), Value::Int(5));
        assert_eq!(run("abs(5);"), Value::Int(5));
        assert_eq!(run("sign(-5);"), Value::Int(-1));
        assert_eq!(run("sign(5);"), Value::Int(1));
        assert_eq!(run("sign(0);"), Value::Int(0));
    }

    #[test]
    fn test_min_max() {
        assert_eq!(run("min(3, 1, 2);"), Value::Int(1));
        assert_eq!(run("max(3, 1, 2);"), Value::Int(3));
        assert_eq!(run("min([3, 1, 2]);"), Value::Int(1));
        assert_eq!(run("max([3, 1, 2]);"), Value::Int(3));
    }

    #[test]
    fn test_rounding() {
        assert_eq!(run("floor(3.7);"), Value::Int(3));
        assert_eq!(run("ceil(3.2);"), Value::Int(4));
        assert_eq!(run("round(3.5);"), Value::Int(4));
        assert_eq!(run("trunc(3.7);"), Value::Int(3));
        assert_eq!(run("trunc(-3.7);"), Value::Int(-3));
    }

    #[test]
    fn test_roots() {
        if let Value::Float(f) = run("sqrt(16);") {
            assert!(approx_eq(f, 4.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("cbrt(27);") {
            assert!(approx_eq(f, 3.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_pow_exp_log() {
        assert_eq!(run("pow(2, 3);"), Value::Int(8));

        if let Value::Float(f) = run("exp(1);") {
            assert!(approx_eq(f, std::f64::consts::E));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("log(E);") {
            assert!(approx_eq(f, 1.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("log10(100);") {
            assert!(approx_eq(f, 2.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("log2(8);") {
            assert!(approx_eq(f, 3.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_trig() {
        if let Value::Float(f) = run("sin(0);") {
            assert!(approx_eq(f, 0.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("cos(0);") {
            assert!(approx_eq(f, 1.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("tan(0);") {
            assert!(approx_eq(f, 0.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_inverse_trig() {
        if let Value::Float(f) = run("asin(0);") {
            assert!(approx_eq(f, 0.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("acos(1);") {
            assert!(approx_eq(f, 0.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("atan(0);") {
            assert!(approx_eq(f, 0.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_atan2() {
        if let Value::Float(f) = run("atan2(1, 1);") {
            assert!(approx_eq(f, std::f64::consts::PI / 4.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_hyperbolic() {
        if let Value::Float(f) = run("sinh(0);") {
            assert!(approx_eq(f, 0.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("cosh(0);") {
            assert!(approx_eq(f, 1.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("tanh(0);") {
            assert!(approx_eq(f, 0.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_random() {
        if let Value::Float(f) = run("random();") {
            assert!(f >= 0.0 && f < 1.0);
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_clamp_lerp() {
        if let Value::Float(f) = run("clamp(5, 0, 10);") {
            assert!(approx_eq(f, 5.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("clamp(-5, 0, 10);") {
            assert!(approx_eq(f, 0.0));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("lerp(0, 10, 0.5);") {
            assert!(approx_eq(f, 5.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_angle_conversion() {
        if let Value::Float(f) = run("deg_to_rad(180);") {
            assert!(approx_eq(f, std::f64::consts::PI));
        } else {
            panic!("Expected float");
        }

        if let Value::Float(f) = run("rad_to_deg(PI);") {
            assert!(approx_eq(f, 180.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_hypot() {
        if let Value::Float(f) = run("hypot(3, 4);") {
            assert!(approx_eq(f, 5.0));
        } else {
            panic!("Expected float");
        }
    }

    #[test]
    fn test_mod_gcd_lcm() {
        assert_eq!(run("mod(7, 3);"), Value::Int(1));
        assert_eq!(run("mod(-7, 3);"), Value::Int(2)); // rem_euclid
        assert_eq!(run("gcd(12, 8);"), Value::Int(4));
        assert_eq!(run("lcm(4, 6);"), Value::Int(12));
    }

    #[test]
    fn test_constants() {
        let vs = VoidScript::new();
        let pi = vs.get_var("PI").unwrap();
        if let Value::Float(f) = pi {
            assert!(approx_eq(f, std::f64::consts::PI));
        } else {
            panic!("Expected float");
        }

        let e = vs.get_var("E").unwrap();
        if let Value::Float(f) = e {
            assert!(approx_eq(f, std::f64::consts::E));
        } else {
            panic!("Expected float");
        }
    }
}
