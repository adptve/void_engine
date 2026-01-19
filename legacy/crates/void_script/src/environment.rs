//! Environment for VoidScript
//!
//! Manages variable scopes and lookups.

use std::collections::HashMap;
use std::sync::Arc;
use parking_lot::RwLock;

use crate::value::Value;

/// Variable scope
#[derive(Debug, Clone)]
pub struct Scope {
    /// Variables in this scope
    variables: HashMap<String, Value>,
}

impl Scope {
    /// Create a new empty scope
    pub fn new() -> Self {
        Self {
            variables: HashMap::new(),
        }
    }

    /// Define a variable in this scope
    pub fn define(&mut self, name: impl Into<String>, value: Value) {
        self.variables.insert(name.into(), value);
    }

    /// Get a variable from this scope
    pub fn get(&self, name: &str) -> Option<&Value> {
        self.variables.get(name)
    }

    /// Set a variable in this scope
    pub fn set(&mut self, name: &str, value: Value) -> bool {
        if self.variables.contains_key(name) {
            self.variables.insert(name.to_string(), value);
            true
        } else {
            false
        }
    }

    /// Check if variable exists in this scope
    pub fn contains(&self, name: &str) -> bool {
        self.variables.contains_key(name)
    }

    /// Get all variable names
    pub fn names(&self) -> impl Iterator<Item = &String> {
        self.variables.keys()
    }

    /// Get all variables
    pub fn variables(&self) -> &HashMap<String, Value> {
        &self.variables
    }
}

impl Default for Scope {
    fn default() -> Self {
        Self::new()
    }
}

/// Environment with nested scopes
#[derive(Clone)]
pub struct Environment {
    /// Stack of scopes (innermost last)
    scopes: Vec<Scope>,
    /// Global scope (shared between interpreter runs)
    globals: Arc<RwLock<Scope>>,
}

impl Environment {
    /// Create a new environment with a global scope
    pub fn new() -> Self {
        Self {
            scopes: vec![Scope::new()],
            globals: Arc::new(RwLock::new(Scope::new())),
        }
    }

    /// Create an environment with an existing global scope
    pub fn with_globals(globals: Arc<RwLock<Scope>>) -> Self {
        Self {
            scopes: vec![Scope::new()],
            globals,
        }
    }

    /// Push a new scope
    pub fn push_scope(&mut self) {
        self.scopes.push(Scope::new());
    }

    /// Pop the innermost scope
    pub fn pop_scope(&mut self) {
        if self.scopes.len() > 1 {
            self.scopes.pop();
        }
    }

    /// Get current scope depth
    pub fn depth(&self) -> usize {
        self.scopes.len()
    }

    /// Define a variable in the current scope
    pub fn define(&mut self, name: impl Into<String>, value: Value) {
        if let Some(scope) = self.scopes.last_mut() {
            scope.define(name, value);
        }
    }

    /// Define a global variable
    pub fn define_global(&self, name: impl Into<String>, value: Value) {
        self.globals.write().define(name, value);
    }

    /// Get a variable value (searches all scopes)
    pub fn get(&self, name: &str) -> Option<Value> {
        // Search local scopes from innermost to outermost
        for scope in self.scopes.iter().rev() {
            if let Some(value) = scope.get(name) {
                return Some(value.clone());
            }
        }

        // Check globals
        self.globals.read().get(name).cloned()
    }

    /// Set a variable value (searches all scopes)
    pub fn set(&mut self, name: &str, value: Value) -> bool {
        // Search local scopes from innermost to outermost
        for scope in self.scopes.iter_mut().rev() {
            if scope.set(name, value.clone()) {
                return true;
            }
        }

        // Check globals
        self.globals.write().set(name, value)
    }

    /// Check if a variable exists
    pub fn contains(&self, name: &str) -> bool {
        // Check local scopes
        for scope in self.scopes.iter().rev() {
            if scope.contains(name) {
                return true;
            }
        }

        // Check globals
        self.globals.read().contains(name)
    }

    /// Get the global scope
    pub fn globals(&self) -> &Arc<RwLock<Scope>> {
        &self.globals
    }

    /// Get the current (innermost) scope
    pub fn current_scope(&self) -> Option<&Scope> {
        self.scopes.last()
    }

    /// Get a mutable reference to the current scope
    pub fn current_scope_mut(&mut self) -> Option<&mut Scope> {
        self.scopes.last_mut()
    }

    /// Create a snapshot of all current variables
    pub fn snapshot(&self) -> HashMap<String, Value> {
        let mut result = HashMap::new();

        // Add globals first
        for (k, v) in self.globals.read().variables() {
            result.insert(k.clone(), v.clone());
        }

        // Add local scopes (later ones override earlier)
        for scope in &self.scopes {
            for (k, v) in scope.variables() {
                result.insert(k.clone(), v.clone());
            }
        }

        result
    }

    /// Create a child environment for function calls
    pub fn child(&self) -> Self {
        Self {
            scopes: vec![Scope::new()],
            globals: Arc::clone(&self.globals),
        }
    }
}

impl Default for Environment {
    fn default() -> Self {
        Self::new()
    }
}

impl std::fmt::Debug for Environment {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Environment")
            .field("scope_depth", &self.scopes.len())
            .field("local_vars", &self.scopes.iter().flat_map(|s| s.names()).count())
            .field("global_vars", &self.globals.read().names().count())
            .finish()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_scope_define_get() {
        let mut scope = Scope::new();
        scope.define("x", Value::Int(42));
        assert_eq!(scope.get("x"), Some(&Value::Int(42)));
    }

    #[test]
    fn test_scope_set() {
        let mut scope = Scope::new();
        scope.define("x", Value::Int(1));
        assert!(scope.set("x", Value::Int(2)));
        assert_eq!(scope.get("x"), Some(&Value::Int(2)));
        assert!(!scope.set("y", Value::Int(3)));
    }

    #[test]
    fn test_environment_scoping() {
        let mut env = Environment::new();

        // Define in outer scope
        env.define("x", Value::Int(1));
        assert_eq!(env.get("x"), Some(Value::Int(1)));

        // Push scope and shadow
        env.push_scope();
        env.define("x", Value::Int(2));
        assert_eq!(env.get("x"), Some(Value::Int(2)));

        // Pop scope, original value restored
        env.pop_scope();
        assert_eq!(env.get("x"), Some(Value::Int(1)));
    }

    #[test]
    fn test_environment_set_outer() {
        let mut env = Environment::new();

        env.define("x", Value::Int(1));
        env.push_scope();

        // Set should find outer scope variable
        assert!(env.set("x", Value::Int(2)));

        env.pop_scope();
        assert_eq!(env.get("x"), Some(Value::Int(2)));
    }

    #[test]
    fn test_environment_globals() {
        let env = Environment::new();
        env.define_global("GLOBAL", Value::String("test".to_string()));

        // Create child environment
        let child = env.child();
        assert_eq!(child.get("GLOBAL"), Some(Value::String("test".to_string())));
    }

    #[test]
    fn test_environment_snapshot() {
        let mut env = Environment::new();
        env.define_global("g", Value::Int(1));
        env.define("x", Value::Int(2));
        env.push_scope();
        env.define("y", Value::Int(3));

        let snapshot = env.snapshot();
        assert_eq!(snapshot.get("g"), Some(&Value::Int(1)));
        assert_eq!(snapshot.get("x"), Some(&Value::Int(2)));
        assert_eq!(snapshot.get("y"), Some(&Value::Int(3)));
    }

    #[test]
    fn test_environment_child_isolation() {
        let mut env = Environment::new();
        env.define("x", Value::Int(1));

        let mut child = env.child();
        child.define("y", Value::Int(2));

        // Child has its own local scope
        assert_eq!(child.get("y"), Some(Value::Int(2)));
        // But original env doesn't see it
        assert!(env.get("y").is_none());
    }
}
