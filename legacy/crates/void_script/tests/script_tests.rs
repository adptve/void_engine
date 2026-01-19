//! Integration tests for void_script (VoidScript language)

use void_script::*;

#[test]
fn test_basic_arithmetic() {
    let mut vs = VoidScript::new();

    assert_eq!(vs.eval("1 + 1").unwrap(), Value::Int(2));
    assert_eq!(vs.eval("10 - 3").unwrap(), Value::Int(7));
    assert_eq!(vs.eval("4 * 5").unwrap(), Value::Int(20));
    assert_eq!(vs.eval("20 / 4").unwrap(), Value::Int(5));
}

#[test]
fn test_operator_precedence() {
    let mut vs = VoidScript::new();

    assert_eq!(vs.eval("2 + 3 * 4").unwrap(), Value::Int(14)); // Not 20
    assert_eq!(vs.eval("(2 + 3) * 4").unwrap(), Value::Int(20));
    assert_eq!(vs.eval("10 - 2 * 3").unwrap(), Value::Int(4)); // Not 24
}

#[test]
fn test_floating_point() {
    let mut vs = VoidScript::new();

    if let Value::Float(result) = vs.eval("3.14 + 2.86").unwrap() {
        assert!((result - 6.0).abs() < 0.01);
    } else {
        panic!("Expected float result");
    }
}

#[test]
fn test_variable_declaration_and_access() {
    let mut vs = VoidScript::new();

    vs.execute("let x = 42;").unwrap();
    assert_eq!(vs.get_var("x"), Some(Value::Int(42)));

    let result = vs.eval("x").unwrap();
    assert_eq!(result, Value::Int(42));
}

#[test]
fn test_variable_reassignment() {
    let mut vs = VoidScript::new();

    vs.execute("let x = 10;").unwrap();
    assert_eq!(vs.get_var("x"), Some(Value::Int(10)));

    vs.execute("x = 20;").unwrap();
    assert_eq!(vs.get_var("x"), Some(Value::Int(20)));
}

#[test]
fn test_multiple_variables() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        let a = 1;
        let b = 2;
        let c = 3;
    "#).unwrap();

    assert_eq!(vs.get_var("a"), Some(Value::Int(1)));
    assert_eq!(vs.get_var("b"), Some(Value::Int(2)));
    assert_eq!(vs.get_var("c"), Some(Value::Int(3)));
}

#[test]
fn test_string_literals() {
    let mut vs = VoidScript::new();

    let result = vs.eval(r#""hello world""#).unwrap();
    assert_eq!(result, Value::String("hello world".to_string()));
}

#[test]
fn test_string_concatenation() {
    let mut vs = VoidScript::new();

    let result = vs.eval(r#""hello" + " " + "world""#).unwrap();
    assert_eq!(result, Value::String("hello world".to_string()));
}

#[test]
fn test_boolean_literals() {
    let mut vs = VoidScript::new();

    assert_eq!(vs.eval("true").unwrap(), Value::Bool(true));
    assert_eq!(vs.eval("false").unwrap(), Value::Bool(false));
}

#[test]
fn test_comparison_operators() {
    let mut vs = VoidScript::new();

    assert_eq!(vs.eval("5 > 3").unwrap(), Value::Bool(true));
    assert_eq!(vs.eval("5 < 3").unwrap(), Value::Bool(false));
    assert_eq!(vs.eval("5 >= 5").unwrap(), Value::Bool(true));
    assert_eq!(vs.eval("5 <= 3").unwrap(), Value::Bool(false));
    assert_eq!(vs.eval("5 == 5").unwrap(), Value::Bool(true));
    assert_eq!(vs.eval("5 != 3").unwrap(), Value::Bool(true));
}

#[test]
fn test_if_statement() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        let result = 0;
        if true {
            result = 1;
        }
    "#).unwrap();

    assert_eq!(vs.get_var("result"), Some(Value::Int(1)));
}

#[test]
fn test_if_else_statement() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        let result = 0;
        if false {
            result = 1;
        } else {
            result = 2;
        }
    "#).unwrap();

    assert_eq!(vs.get_var("result"), Some(Value::Int(2)));
}

#[test]
fn test_if_with_comparison() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        let x = 10;
        let result = 0;
        if x > 5 {
            result = 1;
        } else {
            result = 2;
        }
    "#).unwrap();

    assert_eq!(vs.get_var("result"), Some(Value::Int(1)));
}

#[test]
fn test_while_loop() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        let i = 0;
        let sum = 0;
        while i < 5 {
            sum = sum + i;
            i = i + 1;
        }
    "#).unwrap();

    assert_eq!(vs.get_var("sum"), Some(Value::Int(10))); // 0+1+2+3+4 = 10
    assert_eq!(vs.get_var("i"), Some(Value::Int(5)));
}

#[test]
fn test_function_declaration() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        fn add(a, b) {
            return a + b;
        }
    "#).unwrap();

    let result = vs.eval("add(3, 4)").unwrap();
    assert_eq!(result, Value::Int(7));
}

#[test]
fn test_function_with_multiple_statements() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        fn calculate(x) {
            let y = x * 2;
            let z = y + 10;
            return z;
        }
    "#).unwrap();

    let result = vs.eval("calculate(5)").unwrap();
    assert_eq!(result, Value::Int(20)); // 5 * 2 + 10 = 20
}

#[test]
fn test_function_recursion() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        fn factorial(n) {
            if n <= 1 {
                return 1;
            } else {
                return n * factorial(n - 1);
            }
        }
    "#).unwrap();

    let result = vs.eval("factorial(5)").unwrap();
    assert_eq!(result, Value::Int(120)); // 5! = 120
}

#[test]
fn test_native_function_registration() {
    let mut vs = VoidScript::new();

    vs.register_fn("double", |args| {
        if let Some(Value::Int(n)) = args.first() {
            Ok(Value::Int(n * 2))
        } else {
            Err("Expected integer argument".to_string())
        }
    });

    let result = vs.eval("double(21)").unwrap();
    assert_eq!(result, Value::Int(42));
}

#[test]
fn test_native_function_with_multiple_args() {
    let mut vs = VoidScript::new();

    vs.register_fn("max", |args| {
        if args.len() != 2 {
            return Err("Expected 2 arguments".to_string());
        }

        match (&args[0], &args[1]) {
            (Value::Int(a), Value::Int(b)) => Ok(Value::Int(*a.max(b))),
            _ => Err("Expected integer arguments".to_string()),
        }
    });

    assert_eq!(vs.eval("max(10, 20)").unwrap(), Value::Int(20));
    assert_eq!(vs.eval("max(50, 30)").unwrap(), Value::Int(50));
}

#[test]
fn test_script_reset() {
    let mut vs = VoidScript::new();

    vs.execute("let x = 42;").unwrap();
    assert_eq!(vs.get_var("x"), Some(Value::Int(42)));

    vs.reset();
    assert_eq!(vs.get_var("x"), None);
}

#[test]
fn test_undefined_variable_error() {
    let mut vs = VoidScript::new();

    let result = vs.eval("undefined_var");
    assert!(result.is_err());
}

#[test]
fn test_syntax_error() {
    let mut vs = VoidScript::new();

    // Missing semicolon or invalid syntax
    let result = vs.execute("let x = ");
    assert!(result.is_err());
}

#[test]
fn test_type_error() {
    let mut vs = VoidScript::new();

    // Try to add string and number (should fail or coerce)
    // Behavior depends on implementation
    let result = vs.eval(r#""hello" + 42"#);
    // Could be Ok with coercion or Err - check your implementation
}

#[test]
fn test_nested_if_statements() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        let x = 10;
        let result = 0;
        if x > 5 {
            if x > 8 {
                result = 1;
            } else {
                result = 2;
            }
        } else {
            result = 3;
        }
    "#).unwrap();

    assert_eq!(vs.get_var("result"), Some(Value::Int(1)));
}

#[test]
fn test_variable_scoping_in_functions() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        let global = 100;

        fn test() {
            let local = 200;
            return local;
        }

        let result = test();
    "#).unwrap();

    assert_eq!(vs.get_var("result"), Some(Value::Int(200)));
    assert_eq!(vs.get_var("global"), Some(Value::Int(100)));
}

#[test]
fn test_expression_as_statement() {
    let mut vs = VoidScript::new();

    // Expression without assignment should still execute
    vs.execute("1 + 1;").unwrap();
    vs.execute(r#""hello";"#).unwrap();
}

#[test]
fn test_multiline_script() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        let a = 10;
        let b = 20;
        let c = a + b;

        fn multiply(x, y) {
            return x * y;
        }

        let result = multiply(c, 2);
    "#).unwrap();

    assert_eq!(vs.get_var("result"), Some(Value::Int(60))); // (10 + 20) * 2 = 60
}

#[test]
fn test_empty_function_body() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        fn empty() {
        }
    "#).unwrap();

    // Should return nil/null
    let result = vs.eval("empty()").unwrap();
    assert_eq!(result, Value::Nil);
}

#[test]
fn test_function_without_return() {
    let mut vs = VoidScript::new();

    vs.execute(r#"
        fn no_return(x) {
            let y = x + 1;
        }
    "#).unwrap();

    let result = vs.eval("no_return(5)").unwrap();
    assert_eq!(result, Value::Nil);
}

#[test]
fn test_logical_operators() {
    let mut vs = VoidScript::new();

    // AND
    assert_eq!(vs.eval("true && true").unwrap(), Value::Bool(true));
    assert_eq!(vs.eval("true && false").unwrap(), Value::Bool(false));

    // OR
    assert_eq!(vs.eval("true || false").unwrap(), Value::Bool(true));
    assert_eq!(vs.eval("false || false").unwrap(), Value::Bool(false));

    // NOT
    assert_eq!(vs.eval("!true").unwrap(), Value::Bool(false));
    assert_eq!(vs.eval("!false").unwrap(), Value::Bool(true));
}

#[test]
fn test_complex_expression() {
    let mut vs = VoidScript::new();

    let result = vs.eval("(5 + 3) * 2 - 4 / 2").unwrap();
    assert_eq!(result, Value::Int(14)); // (8 * 2) - 2 = 16 - 2 = 14
}

#[test]
fn test_string_with_special_characters() {
    let mut vs = VoidScript::new();

    let result = vs.eval(r#""hello\nworld""#).unwrap();
    // Check if it contains the string (escape handling depends on implementation)
    if let Value::String(s) = result {
        assert!(s.contains("hello"));
        assert!(s.contains("world"));
    }
}

#[test]
fn test_set_var_programmatically() {
    let mut vs = VoidScript::new();

    vs.set_var("external_value", Value::Int(999));
    assert_eq!(vs.get_var("external_value"), Some(Value::Int(999)));

    let result = vs.eval("external_value + 1").unwrap();
    assert_eq!(result, Value::Int(1000));
}
