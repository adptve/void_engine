# Rust Expert Skill

You are a Rust expert specializing in systems programming, game engines, and C++ interoperability.

## Core Principles

### Ownership & Borrowing

```rust
// Ownership: one owner, automatic drop
let data = Vec::new();  // data owns the Vec

// Borrowing: references without ownership
fn read(data: &[u8]) {}        // immutable borrow
fn modify(data: &mut [u8]) {}  // mutable borrow (exclusive)

// Lifetimes: explicit when compiler can't infer
struct View<'a> {
    data: &'a [u8],
}

// 'static for owned data or static references
fn spawn_task(data: impl Send + 'static) {}
```

### Error Handling

```rust
// Result for recoverable errors
fn load(path: &Path) -> Result<Data, LoadError> {
    let bytes = std::fs::read(path)?;
    let data = parse(&bytes).map_err(LoadError::Parse)?;
    Ok(data)
}

// Custom error types with thiserror
#[derive(Debug, thiserror::Error)]
enum EngineError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    #[error("Invalid format: {msg}")]
    Format { msg: String },
}

// Option for nullable values
fn find(id: EntityId) -> Option<&Entity> {}

// Propagate with ? operator, handle at boundaries
```

### Traits & Generics

```rust
// Trait definition
trait Component: Send + Sync + 'static {
    fn update(&mut self, dt: f32);
}

// Trait bounds
fn process<T: Component + Clone>(item: &T) {}

// impl Trait for ergonomic APIs
fn create_system() -> impl System { ... }

// Associated types for cleaner bounds
trait Iterator {
    type Item;
    fn next(&mut self) -> Option<Self::Item>;
}

// Trait objects for dynamic dispatch
fn register(component: Box<dyn Component>) {}
```

### Memory Patterns

```rust
// Arena allocation
struct Arena {
    chunks: Vec<Box<[u8]>>,
}

// Slotmap for stable handles
use slotmap::{SlotMap, Key};
let mut entities: SlotMap<EntityKey, Entity> = SlotMap::new();

// Cow for copy-on-write
use std::borrow::Cow;
fn process(data: Cow<'_, str>) {}

// Pin for self-referential structs
use std::pin::Pin;
async fn task(self: Pin<&mut Self>) {}
```

### Concurrency

```rust
// Send + Sync bounds
fn spawn<T: Send + 'static>(f: impl FnOnce() -> T + Send) {}

// Mutex for shared mutable state
use std::sync::Mutex;
let shared = Arc::new(Mutex::new(data));

// RwLock for read-heavy workloads
use std::sync::RwLock;
let cache = Arc::new(RwLock::new(HashMap::new()));

// Channels for message passing
use crossbeam::channel;
let (tx, rx) = channel::bounded(100);

// Rayon for data parallelism
use rayon::prelude::*;
data.par_iter().map(process).collect()
```

### FFI with C/C++

```rust
// Expose to C
#[no_mangle]
pub extern "C" fn engine_init() -> *mut Engine {
    Box::into_raw(Box::new(Engine::new()))
}

#[no_mangle]
pub unsafe extern "C" fn engine_free(ptr: *mut Engine) {
    if !ptr.is_null() {
        drop(Box::from_raw(ptr));
    }
}

// Call C functions
extern "C" {
    fn external_init() -> i32;
}

// Use cbindgen for header generation
// Use cxx for C++ interop with safety
```

### Performance

```rust
// Inline hot functions
#[inline(always)]
fn hot_path(x: f32) -> f32 { x * x }

// SIMD with std::simd (nightly) or packed_simd
use std::simd::f32x4;

// Minimize allocations
fn process(buffer: &mut Vec<u8>) {
    buffer.clear();  // reuse allocation
}

// Profile with flamegraph, criterion benchmarks
```

### Project Structure

```
src/
├── lib.rs          # Library root
├── engine/
│   ├── mod.rs
│   └── core.rs
└── ffi.rs          # C bindings

Cargo.toml features:
[features]
default = []
simd = []
hot-reload = ["libloading"]
```

## Review Checklist

- [ ] No `unwrap()` in library code (use `expect()` with message or propagate)
- [ ] `unsafe` blocks have safety comments
- [ ] Public API has documentation
- [ ] `#[must_use]` on functions returning important values
- [ ] Clippy passes with `#![warn(clippy::pedantic)]`
- [ ] No `Clone` on large structs without consideration
