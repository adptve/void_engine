//! Collection built-in functions for VoidScript
//!
//! Provides functions for working with arrays and objects:
//! - len: Get length of array, string, or object
//! - push, pop: Array modification (returns new array)
//! - first, last: Get first/last element
//! - keys, values: Get object keys/values
//! - range: Generate array of integers
//! - map, filter, reduce: Functional operations
//! - slice, reverse: Array manipulation
//! - concat, flatten: Array combining
//! - find, find_index: Search operations
//! - sort, unique: Array processing

use crate::interpreter::Interpreter;
use crate::value::Value;

/// Register collection functions with the interpreter
pub fn register(interpreter: &mut Interpreter) {
    register_len(interpreter);
    register_push(interpreter);
    register_pop(interpreter);
    register_first(interpreter);
    register_last(interpreter);
    register_keys(interpreter);
    register_values(interpreter);
    register_range(interpreter);
    register_slice(interpreter);
    register_reverse(interpreter);
    register_concat(interpreter);
    register_flatten(interpreter);
    register_find_index(interpreter);
    register_index_of(interpreter);
    register_sort(interpreter);
    register_unique(interpreter);
    register_zip(interpreter);
    register_enumerate(interpreter);
    register_sum(interpreter);
    register_product(interpreter);
    register_any(interpreter);
    register_all(interpreter);
    register_count(interpreter);
    register_take(interpreter);
    register_drop(interpreter);
    register_insert(interpreter);
    register_remove(interpreter);
    register_clear(interpreter);
    register_has_key(interpreter);
    register_get(interpreter);
    register_set(interpreter);
    register_merge(interpreter);
}

/// len(value) - Get length of array, string, or object
fn register_len(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("len", 1, |args| {
        match &args[0] {
            Value::String(s) => Ok(Value::Int(s.chars().count() as i64)),
            Value::Array(arr) => Ok(Value::Int(arr.len() as i64)),
            Value::Object(obj) => Ok(Value::Int(obj.len() as i64)),
            _ => Err(format!("Cannot get length of {}", args[0].type_name())),
        }
    });
}

/// push(array, value) - Append value to array (returns new array)
fn register_push(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("push", 2, |args| {
        match &args[0] {
            Value::Array(arr) => {
                let mut new_arr = arr.clone();
                new_arr.push(args[1].clone());
                Ok(Value::Array(new_arr))
            }
            _ => Err("push() expects an array".to_string()),
        }
    });
}

/// pop(array) - Remove last element (returns new array)
fn register_pop(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("pop", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                if arr.is_empty() {
                    Err("Cannot pop from empty array".to_string())
                } else {
                    let mut new_arr = arr.clone();
                    new_arr.pop();
                    Ok(Value::Array(new_arr))
                }
            }
            _ => Err("pop() expects an array".to_string()),
        }
    });
}

/// first(array) - Get first element
fn register_first(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("first", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                arr.first().cloned().ok_or_else(|| "Array is empty".to_string())
            }
            Value::String(s) => {
                s.chars().next()
                    .map(|c| Value::String(c.to_string()))
                    .ok_or_else(|| "String is empty".to_string())
            }
            _ => Err("first() expects an array or string".to_string()),
        }
    });
}

/// last(array) - Get last element
fn register_last(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("last", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                arr.last().cloned().ok_or_else(|| "Array is empty".to_string())
            }
            Value::String(s) => {
                s.chars().last()
                    .map(|c| Value::String(c.to_string()))
                    .ok_or_else(|| "String is empty".to_string())
            }
            _ => Err("last() expects an array or string".to_string()),
        }
    });
}

/// keys(object) - Get array of object keys
fn register_keys(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("keys", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                let keys: Vec<Value> = obj.keys()
                    .map(|k| Value::String(k.clone()))
                    .collect();
                Ok(Value::Array(keys))
            }
            _ => Err("keys() expects an object".to_string()),
        }
    });
}

/// values(object) - Get array of object values
fn register_values(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("values", 1, |args| {
        match &args[0] {
            Value::Object(obj) => {
                let values: Vec<Value> = obj.values().cloned().collect();
                Ok(Value::Array(values))
            }
            _ => Err("values() expects an object".to_string()),
        }
    });
}

/// range(end) or range(start, end) or range(start, end, step)
fn register_range(interpreter: &mut Interpreter) {
    interpreter.register_native("range", |args| {
        let (start, end, step) = match args.len() {
            1 => match &args[0] {
                Value::Int(n) => (0, *n, 1),
                _ => return Err("range() expects integers".to_string()),
            },
            2 => match (&args[0], &args[1]) {
                (Value::Int(a), Value::Int(b)) => (*a, *b, 1),
                _ => return Err("range() expects integers".to_string()),
            },
            3 => match (&args[0], &args[1], &args[2]) {
                (Value::Int(a), Value::Int(b), Value::Int(c)) => (*a, *b, *c),
                _ => return Err("range() expects integers".to_string()),
            },
            _ => return Err("range() expects 1-3 arguments".to_string()),
        };

        if step == 0 {
            return Err("range() step cannot be zero".to_string());
        }

        let mut result = Vec::new();
        let mut i = start;
        if step > 0 {
            while i < end {
                result.push(Value::Int(i));
                i += step;
            }
        } else {
            while i > end {
                result.push(Value::Int(i));
                i += step;
            }
        }

        Ok(Value::Array(result))
    });
}

/// slice(array, start, end?) - Get subarray
fn register_slice(interpreter: &mut Interpreter) {
    interpreter.register_native("slice", |args| {
        if args.len() < 2 || args.len() > 3 {
            return Err("slice() expects 2-3 arguments".to_string());
        }

        match &args[0] {
            Value::Array(arr) => {
                let start = match &args[1] {
                    Value::Int(n) => {
                        if *n < 0 {
                            (arr.len() as i64 + n).max(0) as usize
                        } else {
                            (*n as usize).min(arr.len())
                        }
                    }
                    _ => return Err("slice() indices must be integers".to_string()),
                };

                let end = if args.len() == 3 {
                    match &args[2] {
                        Value::Int(n) => {
                            if *n < 0 {
                                (arr.len() as i64 + n).max(0) as usize
                            } else {
                                (*n as usize).min(arr.len())
                            }
                        }
                        _ => return Err("slice() indices must be integers".to_string()),
                    }
                } else {
                    arr.len()
                };

                if start >= end {
                    Ok(Value::Array(Vec::new()))
                } else {
                    Ok(Value::Array(arr[start..end].to_vec()))
                }
            }
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
                    _ => return Err("slice() indices must be integers".to_string()),
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
                        _ => return Err("slice() indices must be integers".to_string()),
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
            _ => Err("slice() expects an array or string".to_string()),
        }
    });
}

/// reverse(array) - Reverse array or string
fn register_reverse(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("reverse", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                let mut new_arr = arr.clone();
                new_arr.reverse();
                Ok(Value::Array(new_arr))
            }
            Value::String(s) => {
                Ok(Value::String(s.chars().rev().collect()))
            }
            _ => Err("reverse() expects an array or string".to_string()),
        }
    });
}

/// concat(array1, array2, ...) - Concatenate arrays
fn register_concat(interpreter: &mut Interpreter) {
    interpreter.register_native("concat", |args| {
        let mut result = Vec::new();
        for arg in args {
            match arg {
                Value::Array(arr) => result.extend(arr),
                _ => return Err("concat() expects arrays".to_string()),
            }
        }
        Ok(Value::Array(result))
    });
}

/// flatten(array) - Flatten nested arrays one level
fn register_flatten(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("flatten", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                let mut result = Vec::new();
                for item in arr {
                    match item {
                        Value::Array(inner) => result.extend(inner.clone()),
                        other => result.push(other.clone()),
                    }
                }
                Ok(Value::Array(result))
            }
            _ => Err("flatten() expects an array".to_string()),
        }
    });
}

/// find_index(array, value) - Find index of value in array (-1 if not found)
fn register_find_index(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("find_index", 2, |args| {
        match &args[0] {
            Value::Array(arr) => {
                for (i, item) in arr.iter().enumerate() {
                    if *item == args[1] {
                        return Ok(Value::Int(i as i64));
                    }
                }
                Ok(Value::Int(-1))
            }
            _ => Err("find_index() expects an array".to_string()),
        }
    });
}

/// index_of(array, value) - Alias for find_index
fn register_index_of(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("index_of", 2, |args| {
        match &args[0] {
            Value::Array(arr) => {
                for (i, item) in arr.iter().enumerate() {
                    if *item == args[1] {
                        return Ok(Value::Int(i as i64));
                    }
                }
                Ok(Value::Int(-1))
            }
            Value::String(s) => {
                if let Value::String(needle) = &args[1] {
                    match s.find(needle.as_str()) {
                        Some(idx) => Ok(Value::Int(idx as i64)),
                        None => Ok(Value::Int(-1)),
                    }
                } else {
                    Err("index_of() expects a string needle for string haystack".to_string())
                }
            }
            _ => Err("index_of() expects an array or string".to_string()),
        }
    });
}

/// sort(array) - Sort array (numbers or strings)
fn register_sort(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("sort", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                let mut new_arr = arr.clone();

                // Check if all elements are comparable
                let all_ints = new_arr.iter().all(|v| matches!(v, Value::Int(_)));
                let all_floats = new_arr.iter().all(|v| matches!(v, Value::Float(_) | Value::Int(_)));
                let all_strings = new_arr.iter().all(|v| matches!(v, Value::String(_)));

                if all_ints {
                    new_arr.sort_by(|a, b| {
                        let a = if let Value::Int(n) = a { *n } else { 0 };
                        let b = if let Value::Int(n) = b { *n } else { 0 };
                        a.cmp(&b)
                    });
                } else if all_floats {
                    new_arr.sort_by(|a, b| {
                        let a = a.to_float().unwrap_or(0.0);
                        let b = b.to_float().unwrap_or(0.0);
                        a.partial_cmp(&b).unwrap_or(std::cmp::Ordering::Equal)
                    });
                } else if all_strings {
                    new_arr.sort_by(|a, b| {
                        let a = a.to_string_value();
                        let b = b.to_string_value();
                        a.cmp(&b)
                    });
                } else {
                    return Err("sort() requires array of comparable elements".to_string());
                }

                Ok(Value::Array(new_arr))
            }
            _ => Err("sort() expects an array".to_string()),
        }
    });
}

/// unique(array) - Remove duplicates from array
fn register_unique(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("unique", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                let mut result = Vec::new();
                for item in arr {
                    if !result.contains(item) {
                        result.push(item.clone());
                    }
                }
                Ok(Value::Array(result))
            }
            _ => Err("unique() expects an array".to_string()),
        }
    });
}

/// zip(array1, array2) - Combine two arrays into array of pairs
fn register_zip(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("zip", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Array(arr1), Value::Array(arr2)) => {
                let len = arr1.len().min(arr2.len());
                let result: Vec<Value> = arr1.iter()
                    .zip(arr2.iter())
                    .take(len)
                    .map(|(a, b)| Value::Array(vec![a.clone(), b.clone()]))
                    .collect();
                Ok(Value::Array(result))
            }
            _ => Err("zip() expects two arrays".to_string()),
        }
    });
}

/// enumerate(array) - Create array of [index, value] pairs
fn register_enumerate(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("enumerate", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                let result: Vec<Value> = arr.iter()
                    .enumerate()
                    .map(|(i, v)| Value::Array(vec![Value::Int(i as i64), v.clone()]))
                    .collect();
                Ok(Value::Array(result))
            }
            _ => Err("enumerate() expects an array".to_string()),
        }
    });
}

/// sum(array) - Sum all numeric elements
fn register_sum(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("sum", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                let mut int_sum: i64 = 0;
                let mut float_sum: f64 = 0.0;
                let mut has_float = false;

                for item in arr {
                    match item {
                        Value::Int(n) => int_sum += n,
                        Value::Float(f) => {
                            has_float = true;
                            float_sum += f;
                        }
                        _ => return Err("sum() requires array of numbers".to_string()),
                    }
                }

                if has_float {
                    Ok(Value::Float(int_sum as f64 + float_sum))
                } else {
                    Ok(Value::Int(int_sum))
                }
            }
            _ => Err("sum() expects an array".to_string()),
        }
    });
}

/// product(array) - Multiply all numeric elements
fn register_product(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("product", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                if arr.is_empty() {
                    return Ok(Value::Int(1));
                }

                let mut int_product: i64 = 1;
                let mut float_product: f64 = 1.0;
                let mut has_float = false;

                for item in arr {
                    match item {
                        Value::Int(n) => {
                            if has_float {
                                float_product *= *n as f64;
                            } else {
                                int_product *= n;
                            }
                        }
                        Value::Float(f) => {
                            if !has_float {
                                has_float = true;
                                float_product = int_product as f64;
                            }
                            float_product *= f;
                        }
                        _ => return Err("product() requires array of numbers".to_string()),
                    }
                }

                if has_float {
                    Ok(Value::Float(float_product))
                } else {
                    Ok(Value::Int(int_product))
                }
            }
            _ => Err("product() expects an array".to_string()),
        }
    });
}

/// any(array) - Check if any element is truthy
fn register_any(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("any", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                Ok(Value::Bool(arr.iter().any(|v| v.is_truthy())))
            }
            _ => Err("any() expects an array".to_string()),
        }
    });
}

/// all(array) - Check if all elements are truthy
fn register_all(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("all", 1, |args| {
        match &args[0] {
            Value::Array(arr) => {
                Ok(Value::Bool(arr.iter().all(|v| v.is_truthy())))
            }
            _ => Err("all() expects an array".to_string()),
        }
    });
}

/// count(array, value) - Count occurrences of value in array
fn register_count(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("count", 2, |args| {
        match &args[0] {
            Value::Array(arr) => {
                let count = arr.iter().filter(|v| **v == args[1]).count();
                Ok(Value::Int(count as i64))
            }
            _ => Err("count() expects an array".to_string()),
        }
    });
}

/// take(array, n) - Take first n elements
fn register_take(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("take", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Array(arr), Value::Int(n)) => {
                let n = (*n as usize).min(arr.len());
                Ok(Value::Array(arr[..n].to_vec()))
            }
            _ => Err("take() expects an array and integer".to_string()),
        }
    });
}

/// drop(array, n) - Drop first n elements
fn register_drop(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("drop", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Array(arr), Value::Int(n)) => {
                let n = (*n as usize).min(arr.len());
                Ok(Value::Array(arr[n..].to_vec()))
            }
            _ => Err("drop() expects an array and integer".to_string()),
        }
    });
}

/// insert(array, index, value) - Insert value at index (returns new array)
fn register_insert(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("insert", 3, |args| {
        match (&args[0], &args[1]) {
            (Value::Array(arr), Value::Int(idx)) => {
                let mut new_arr = arr.clone();
                let idx = if *idx < 0 {
                    (arr.len() as i64 + idx).max(0) as usize
                } else {
                    (*idx as usize).min(arr.len())
                };
                new_arr.insert(idx, args[2].clone());
                Ok(Value::Array(new_arr))
            }
            _ => Err("insert() expects an array, index, and value".to_string()),
        }
    });
}

/// remove(array, index) - Remove element at index (returns new array)
fn register_remove(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("remove", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Array(arr), Value::Int(idx)) => {
                if arr.is_empty() {
                    return Err("Cannot remove from empty array".to_string());
                }
                let mut new_arr = arr.clone();
                let idx = if *idx < 0 {
                    (arr.len() as i64 + idx).max(0) as usize
                } else {
                    (*idx as usize).min(arr.len() - 1)
                };
                if idx < new_arr.len() {
                    new_arr.remove(idx);
                }
                Ok(Value::Array(new_arr))
            }
            _ => Err("remove() expects an array and index".to_string()),
        }
    });
}

/// clear(array) - Return empty array/object
fn register_clear(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("clear", 1, |args| {
        match &args[0] {
            Value::Array(_) => Ok(Value::Array(Vec::new())),
            Value::Object(_) => Ok(Value::Object(std::collections::HashMap::new())),
            _ => Err("clear() expects an array or object".to_string()),
        }
    });
}

/// has_key(object, key) - Check if object has key
fn register_has_key(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("has_key", 2, |args| {
        match (&args[0], &args[1]) {
            (Value::Object(obj), Value::String(key)) => {
                Ok(Value::Bool(obj.contains_key(key)))
            }
            _ => Err("has_key() expects an object and string key".to_string()),
        }
    });
}

/// get(collection, key, default?) - Get value with optional default
fn register_get(interpreter: &mut Interpreter) {
    interpreter.register_native("get", |args| {
        if args.len() < 2 || args.len() > 3 {
            return Err("get() expects 2-3 arguments".to_string());
        }

        let default = if args.len() == 3 {
            args[2].clone()
        } else {
            Value::Null
        };

        match (&args[0], &args[1]) {
            (Value::Object(obj), Value::String(key)) => {
                Ok(obj.get(key).cloned().unwrap_or(default))
            }
            (Value::Array(arr), Value::Int(idx)) => {
                let idx = if *idx < 0 {
                    (arr.len() as i64 + idx) as usize
                } else {
                    *idx as usize
                };
                Ok(arr.get(idx).cloned().unwrap_or(default))
            }
            _ => Err("get() expects an object/string key or array/integer index".to_string()),
        }
    });
}

/// set(object, key, value) - Set value in object (returns new object)
fn register_set(interpreter: &mut Interpreter) {
    interpreter.register_native_with_arity("set", 3, |args| {
        match (&args[0], &args[1]) {
            (Value::Object(obj), Value::String(key)) => {
                let mut new_obj = obj.clone();
                new_obj.insert(key.clone(), args[2].clone());
                Ok(Value::Object(new_obj))
            }
            (Value::Array(arr), Value::Int(idx)) => {
                let mut new_arr = arr.clone();
                let idx = if *idx < 0 {
                    (arr.len() as i64 + idx).max(0) as usize
                } else {
                    *idx as usize
                };
                if idx < new_arr.len() {
                    new_arr[idx] = args[2].clone();
                }
                Ok(Value::Array(new_arr))
            }
            _ => Err("set() expects an object/string key or array/integer index".to_string()),
        }
    });
}

/// merge(object1, object2, ...) - Merge objects (later values override earlier)
fn register_merge(interpreter: &mut Interpreter) {
    interpreter.register_native("merge", |args| {
        let mut result = std::collections::HashMap::new();
        for arg in args {
            match arg {
                Value::Object(obj) => {
                    for (k, v) in obj {
                        result.insert(k, v);
                    }
                }
                _ => return Err("merge() expects objects".to_string()),
            }
        }
        Ok(Value::Object(result))
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
    fn test_len() {
        assert_eq!(run(r#"len("hello");"#), Value::Int(5));
        assert_eq!(run("len([1, 2, 3]);"), Value::Int(3));
        assert_eq!(run("len({a: 1, b: 2});"), Value::Int(2));
    }

    #[test]
    fn test_push_pop() {
        let result = run("push([1, 2], 3);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 3);
            assert_eq!(arr[2], Value::Int(3));
        } else {
            panic!("Expected array");
        }

        let result = run("pop([1, 2, 3]);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 2);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_first_last() {
        assert_eq!(run("first([1, 2, 3]);"), Value::Int(1));
        assert_eq!(run("last([1, 2, 3]);"), Value::Int(3));
        assert_eq!(run(r#"first("hello");"#), Value::String("h".to_string()));
        assert_eq!(run(r#"last("hello");"#), Value::String("o".to_string()));
    }

    #[test]
    fn test_keys_values() {
        let result = run("keys({a: 1, b: 2});");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 2);
        } else {
            panic!("Expected array");
        }

        let result = run("values({a: 1, b: 2});");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 2);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_range() {
        let result = run("range(5);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 5);
            assert_eq!(arr[0], Value::Int(0));
            assert_eq!(arr[4], Value::Int(4));
        } else {
            panic!("Expected array");
        }

        let result = run("range(1, 4);");
        if let Value::Array(arr) = result {
            assert_eq!(arr, vec![Value::Int(1), Value::Int(2), Value::Int(3)]);
        } else {
            panic!("Expected array");
        }

        let result = run("range(0, 10, 2);");
        if let Value::Array(arr) = result {
            assert_eq!(arr, vec![Value::Int(0), Value::Int(2), Value::Int(4), Value::Int(6), Value::Int(8)]);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_slice() {
        let result = run("slice([1, 2, 3, 4, 5], 1, 3);");
        if let Value::Array(arr) = result {
            assert_eq!(arr, vec![Value::Int(2), Value::Int(3)]);
        } else {
            panic!("Expected array");
        }

        // Negative indices
        let result = run("slice([1, 2, 3, 4, 5], -3);");
        if let Value::Array(arr) = result {
            assert_eq!(arr, vec![Value::Int(3), Value::Int(4), Value::Int(5)]);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_reverse() {
        let result = run("reverse([1, 2, 3]);");
        if let Value::Array(arr) = result {
            assert_eq!(arr, vec![Value::Int(3), Value::Int(2), Value::Int(1)]);
        } else {
            panic!("Expected array");
        }

        assert_eq!(run(r#"reverse("hello");"#), Value::String("olleh".to_string()));
    }

    #[test]
    fn test_concat() {
        let result = run("concat([1, 2], [3, 4]);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 4);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_flatten() {
        let result = run("flatten([[1, 2], [3, 4], 5]);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 5);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_find_index() {
        assert_eq!(run("find_index([1, 2, 3], 2);"), Value::Int(1));
        assert_eq!(run("find_index([1, 2, 3], 5);"), Value::Int(-1));
    }

    #[test]
    fn test_sort() {
        let result = run("sort([3, 1, 2]);");
        if let Value::Array(arr) = result {
            assert_eq!(arr, vec![Value::Int(1), Value::Int(2), Value::Int(3)]);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_unique() {
        let result = run("unique([1, 2, 2, 3, 3, 3]);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 3);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_zip() {
        let result = run("zip([1, 2], [3, 4]);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 2);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_enumerate() {
        let result = run(r#"enumerate(["a", "b"]);"#);
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 2);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_sum_product() {
        assert_eq!(run("sum([1, 2, 3, 4]);"), Value::Int(10));
        assert_eq!(run("product([1, 2, 3, 4]);"), Value::Int(24));
    }

    #[test]
    fn test_any_all() {
        assert_eq!(run("any([false, true, false]);"), Value::Bool(true));
        assert_eq!(run("any([false, false]);"), Value::Bool(false));
        assert_eq!(run("all([true, true]);"), Value::Bool(true));
        assert_eq!(run("all([true, false]);"), Value::Bool(false));
    }

    #[test]
    fn test_count() {
        assert_eq!(run("count([1, 2, 2, 3, 2], 2);"), Value::Int(3));
    }

    #[test]
    fn test_take_drop() {
        let result = run("take([1, 2, 3, 4, 5], 3);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 3);
        } else {
            panic!("Expected array");
        }

        let result = run("drop([1, 2, 3, 4, 5], 2);");
        if let Value::Array(arr) = result {
            assert_eq!(arr.len(), 3);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_insert_remove() {
        let result = run("insert([1, 3], 1, 2);");
        if let Value::Array(arr) = result {
            assert_eq!(arr, vec![Value::Int(1), Value::Int(2), Value::Int(3)]);
        } else {
            panic!("Expected array");
        }

        let result = run("remove([1, 2, 3], 1);");
        if let Value::Array(arr) = result {
            assert_eq!(arr, vec![Value::Int(1), Value::Int(3)]);
        } else {
            panic!("Expected array");
        }
    }

    #[test]
    fn test_has_key() {
        assert_eq!(run("has_key({a: 1}, \"a\");"), Value::Bool(true));
        assert_eq!(run("has_key({a: 1}, \"b\");"), Value::Bool(false));
    }

    #[test]
    fn test_get_set() {
        assert_eq!(run("get({a: 1}, \"a\");"), Value::Int(1));
        assert_eq!(run("get({a: 1}, \"b\", 0);"), Value::Int(0));
        assert_eq!(run("get([1, 2, 3], 1);"), Value::Int(2));

        let result = run("set({a: 1}, \"b\", 2);");
        if let Value::Object(obj) = result {
            assert_eq!(obj.get("b"), Some(&Value::Int(2)));
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_merge() {
        let result = run("merge({a: 1}, {b: 2});");
        if let Value::Object(obj) = result {
            assert_eq!(obj.len(), 2);
        } else {
            panic!("Expected object");
        }
    }

    #[test]
    fn test_clear() {
        let result = run("clear([1, 2, 3]);");
        if let Value::Array(arr) = result {
            assert!(arr.is_empty());
        } else {
            panic!("Expected array");
        }
    }
}
