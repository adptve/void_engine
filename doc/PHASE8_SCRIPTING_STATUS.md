# Phase 8: Scripting System Status

**Status**: FUNCTIONAL (Core systems operational)
**Date**: 2026-01-28
**Grade**: C+ (Functional but needs production hardening)

---

## Overview

Phase 8 implements four parallel scripting systems:

```
┌─────────────────────────────────────────────────────────────┐
│                    void_engine Scripting                    │
├──────────────────┬──────────────────┬──────────────────────┤
│   void_script    │  void_scripting  │      void_cpp        │
│  (Text Scripts)  │  (WASM Runtime)  │   (C++ Hot-Reload)   │
├──────────────────┼──────────────────┼──────────────────────┤
│ • Lexer          │ • WasmRuntime    │ • Compiler           │
│ • Parser         │ • PluginSystem   │ • ModuleRegistry     │
│ • Interpreter    │ • HostAPI        │ • HotReloader        │
│ • ScriptEngine   │ • WASI imports   │ • ClassRegistry      │
└──────────────────┴──────────────────┴──────────────────────┤
                                                              │
                        void_shell                            │
                    (Debug Console)                           │
                    • 128 commands                            │
                    • Remote debugging                        │
──────────────────────────────────────────────────────────────┘
```

---

## Current Status

### void_script (Text Scripting Language)

**Status**: WORKING

| Component | Status | Notes |
|-----------|--------|-------|
| Lexer | ✓ Working | 47 tokens from test script |
| Parser | ✓ Working | AST with 3 statements parsed successfully |
| Interpreter | ✓ Implemented | Tree-walking interpreter |
| Hot-Reload | ✓ Implemented | File watching + state preservation |

**Syntax Features**:
- Modern Rust/Go style: `if condition { }` (no parens required)
- Also supports C-style: `if (condition) { }`
- Functions: `fn name(params) { }`
- Variables: `let x = value;` / `const y = value;`
- Classes: `class Name { fn method() { } }`
- Lambdas: `(x) => x * 2` or `(x) { return x * 2; }`
- Control flow: if/else, while, for, for-in, match
- Exception handling: try/catch/finally/throw

**Standard Library** (60+ functions):
- String: `upper()`, `lower()`, `split()`, `join()`, `replace()`, `trim()`
- Array: `map()`, `filter()`, `reduce()`, `sort()`, `unique()`, `flatten()`
- Math: All standard math functions, trigonometry, random
- Type: `typeof()`, `is_null()`, `is_string()`, `is_array()`, etc.
- I/O: `print()`, `println()`, `debug()`, `log()`, `warn()`, `error()`

**Test Output**:
```
Lexer: 47 tokens from test script
  Keywords: 5, Identifiers: 12, Operators: 24, Literals: 5
Parser: AST created successfully
  Statements: 3
```

---

### void_scripting (WASM Runtime)

**Status**: INITIALIZED

| Component | Status | Notes |
|-----------|--------|-------|
| WasmRuntime | ✓ Initialized | Backend 0 (interpreter mode) |
| HostAPI | ✓ Registered | Engine bindings available |
| WASI imports | ✓ Registered | File/IO syscalls |
| Engine imports | ✓ Registered | Entity/physics/audio APIs |
| Plugin Registry | ✓ Ready | For loading .wasm modules |

**Configuration**:
- Max memory: 256 pages (16 MB)
- Max stack size: 1024 KB
- Debug info: Enabled

**Purpose**: Sandboxed execution of external plugins compiled to WebAssembly.

---

### void_cpp (C++ Hot-Reload)

**Status**: OPERATIONAL

| Component | Status | Notes |
|-----------|--------|-------|
| Compiler | ✓ Ready | C++20, Debug mode |
| Hot-Reload | ✓ ENABLED | File watching active |
| ModuleRegistry | ✓ Ready | Dynamic library management |
| ClassRegistry | ✓ Ready | Native class instantiation |
| HotReloader | ✓ Started | File watching x2 |

**Features**:
- Runtime C++ compilation
- DLL/SO hot-swapping
- State preservation across reloads
- Symbol enumeration
- Platform-specific handling (MSVC/Clang/GCC)

**FFI Interface**:
```cpp
// Exported from C++ plugins
FfiLibraryInfo void_get_library_info();
const FfiClassInfo* void_get_class_info(uint32_t index);
const FfiClassVTable* void_get_class_vtable(const char* class_name);
```

---

### void_shell (Debug Console)

**Status**: READY

| Component | Status | Notes |
|-----------|--------|-------|
| Shell | ✓ Initialized | Prompt: `void> ` |
| Commands | ✓ 128 registered | Full command set |
| Remote | ○ Not started | Port 9876 configured |

**Purpose**: Runtime debugging, inspection, and command execution.

---

## Architecture Analysis

### What's Working Well

1. **Modular Design**: Each scripting system is independent
2. **Modern Syntax**: Parser supports Rust/Go-style control flow
3. **Hot-Reload Infrastructure**: Both script and C++ hot-reload implemented
4. **WASM Sandboxing**: Secure plugin execution environment
5. **Entity Binding**: Scripts attach to entities with per-entity contexts

### Integration Points

```
Scene Loading (Phase 7)
        │
        ├── ScriptComponent → void_script interpreter
        ├── GraphComponent  → void_graph executor (Phase 7)
        └── CppComponent    → void_cpp class instances
```

### Known Limitations (To Address Later)

| Issue | Impact | Priority |
|-------|--------|----------|
| No void_script ↔ void_graph integration | Can't mix visual + text | Medium |
| Tree-walking interpreter | 10-100x slower than bytecode | Medium |
| No visual debugger | Harder to debug complex scripts | High |
| No C++ reflection generator | Manual binding code required | Medium |
| Scripts are isolated | No composition/inheritance | Low |

---

## File Structure

```
src/
├── script/           # void_script - Text scripting language
│   ├── types.cpp     # Value types, tokens, errors
│   ├── lexer.cpp     # Tokenization
│   ├── parser.cpp    # AST generation (Pratt parser)
│   ├── interpreter.cpp # Tree-walking execution
│   └── engine.cpp    # Script-engine integration
│
├── scripting/        # void_scripting - WASM runtime
│   └── system.cpp    # WasmRuntime, PluginSystem, HostAPI
│
├── cpp/              # void_cpp - C++ hot-reload
│   ├── types.cpp     # FFI types, compiler config
│   ├── compiler.cpp  # C++ compilation
│   ├── module.cpp    # DLL/SO loading
│   ├── instance.cpp  # Class instantiation
│   ├── hot_reload.cpp # File watching, reload logic
│   └── system.cpp    # CppSystem coordination
│
└── shell/            # void_shell - Debug console
    └── shell.cpp     # Command parsing, execution
```

---

## API Examples

### Text Script (void_script)
```javascript
fn fibonacci(n) {
    if n <= 1 {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

let result = fibonacci(10);
print("Fibonacci(10) = " + result);
```

### C++ Hot-Reload Class (void_cpp)
```cpp
// player_controller.cpp
#include <void_engine/cpp/macros.hpp>

class PlayerController {
public:
    void begin_play() { /* Called once */ }
    void tick(float dt) { /* Called every frame */ }
    void on_damage(FfiDamageInfo damage) { /* Event handler */ }

    // Hot-reload state preservation
    size_t serialize(uint8_t* buffer, size_t size);
    bool deserialize(const uint8_t* data, size_t size);
};

VOID_EXPORT_CLASS(PlayerController)
```

### Engine Bindings (available in scripts)
```javascript
// Entity operations
let enemy = spawn("enemy_prefab");
set_position(enemy, vec3(10, 0, 5));
destroy(enemy);

// Events
on("player_death", (data) => {
    print("Player died!");
    emit("show_game_over", {});
});

// Physics
apply_force(entity, vec3(0, 100, 0));
let hit = raycast(origin, direction, 100.0);
```

---

## Next Steps

1. **Phase 9**: Gameplay modules (ai, combat, inventory, gamestate)
2. **Future Enhancement**: Bytecode compiler for void_script
3. **Future Enhancement**: Visual debugger integration
4. **Future Enhancement**: void_script ↔ void_graph bridging

---

## Build Verification

```
Phase 8: Scripting
  [script]
    Lexer: 47 tokens from test script
      Keywords: 5, Identifiers: 12, Operators: 24, Literals: 5
    Parser: AST created successfully
      Statements: 3
  [scripting]
    WASM Runtime: initialized
      Max memory: 256 pages (16 MB)
      Max stack size: 1024 KB
    Plugin Registry: ready
    Host API: engine bindings available
  [cpp]
    Compiler: C++20, Debug mode
    Hot-Reload: ENABLED
    Module Registry: ready
    Class Registry: ready for native scripting
  [shell]
    Shell: initialized with 'void> ' prompt
    Commands: 128 registered
    Remote: port 9876 (not started)
Phase 8 complete
```
