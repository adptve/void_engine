# Module Inventory: [MODULE_NAME]

> **CRITICAL**: Complete this inventory BEFORE making any code changes.
> This document is the CONTRACT for what must be preserved.

## Module Overview

- **Location**: `include/void_engine/[module]/` and `src/[module]/`
- **Namespace**: `void_[module]`
- **Current Build Status**: [ ] Compiles / [ ] Has Errors

---

## File Inventory

### Headers
| File | Lines | Purpose |
|------|-------|---------|
| `file.hpp` | ### | Description |

### Implementations
| File | Lines | Type | Purpose |
|------|-------|------|---------|
| `file.cpp` | ### | REAL/STUB | Description |

---

## Function Inventory

### Public API Functions

| Function | File:Line | Status | Hot-Reload | Must Preserve |
|----------|-----------|--------|------------|---------------|
| `function_name()` | file.cpp:123 | REAL/STUB | Yes/No | YES/NO |

### Hot-Reload Functions (SACRED - Never Delete)

| Function | File:Line | Purpose |
|----------|-----------|---------|
| `snapshot()` | file.cpp:### | Captures state for reload |
| `restore()` | file.cpp:### | Restores state after reload |
| `dehydrate()` | file.cpp:### | Serializes for persistence |
| `rehydrate()` | file.cpp:### | Deserializes from persistence |
| `current_version()` | file.cpp:### | Version tracking |
| `is_compatible()` | file.cpp:### | Compatibility checking |

### Factory Functions

| Function | File:Line | Returns | Real or Stub? |
|----------|-----------|---------|---------------|
| `create_xxx()` | file.cpp:### | Type | REAL/STUB |

---

## Feature Inventory

### Implemented Features (PRESERVE)
- [ ] Feature 1 - `file.cpp:line` - Description
- [ ] Feature 2 - `file.cpp:line` - Description

### Stubbed Features (NEED IMPLEMENTATION)
- [ ] Feature 1 - `file.cpp:line` - Currently returns dummy value
- [ ] Feature 2 - `file.cpp:line` - Empty function body

### Advanced Patterns (NEVER SIMPLIFY)
- [ ] Transaction support - `file.cpp:lines`
- [ ] State machine - `file.cpp:lines`
- [ ] Async/callback - `file.cpp:lines`
- [ ] Error handling chain - `file.cpp:lines`

---

## Integration Points

### Initialization
- [ ] `init()` called from: `file.cpp:line`
- [ ] Connects to real system: YES/NO
- [ ] Currently stubbed: YES/NO

### Render Loop Integration
- [ ] Called in render loop: YES/NO
- [ ] Location: `file.cpp:line`
- [ ] Bypassed for direct calls: YES/NO (explain)

### Other Module Dependencies
| Depends On | How | Status |
|------------|-----|--------|
| module_name | Description | Connected/Disconnected |

---

## Build Errors (if any)

### Error 1
```
error message here
```
- **File**: `file.cpp:line`
- **Cause**: Explanation
- **Proposed Fix**: Fix that PRESERVES functionality
- **What Could Be Lost**: If fixed wrong, we'd lose X

### Error 2
```
error message here
```
- **File**: `file.cpp:line`
- **Cause**: Explanation
- **Proposed Fix**: Fix that PRESERVES functionality

---

## Stub vs Real Analysis

> **GOLDEN RULE**: Always keep the MOST ADVANCED functionality.
> More lines, more features, more error handling = more advanced.

### Functions with BOTH Stub and Real Implementations

| Function | Stub Location | Stub Lines | Real Location | Real Lines | Winner | Why |
|----------|---------------|------------|---------------|------------|--------|-----|
| `func()` | stub.cpp:### | ## | real.cpp:### | ## | REAL/STUB/MERGE | Explain |

### Detailed Comparison (for each duplicate)

#### `function_name()`

**Stub version** (`stub.cpp:###`):
```cpp
// Paste the stub implementation here
```
- Lines: ##
- Features: List what it does
- Error handling: None / Basic / Complete
- Hot-reload: Yes / No
- Edge cases handled: List them

**Real version** (`real.cpp:###`):
```cpp
// Paste the real implementation here
```
- Lines: ##
- Features: List what it does
- Error handling: None / Basic / Complete
- Hot-reload: Yes / No
- Edge cases handled: List them

**Decision**: KEEP [version] because:
- [ ] More complete implementation
- [ ] Has hot-reload support
- [ ] Better error handling
- [ ] Handles more edge cases
- [ ] More features

**If MERGE needed**: List what to take from each:
- From stub: feature X
- From real: feature Y, Z

### Stub-Only Functions (Need Real Implementation)

| Function | Stub Location | What It Should Do |
|----------|---------------|-------------------|
| `func()` | stub.cpp:### | Description of real behavior |

### Real-Only Functions (Verify Not Lost)

| Function | Real Location | Critical? | Notes |
|----------|---------------|-----------|-------|
| `func()` | real.cpp:### | YES/NO | Must be preserved |

---

## Post-Fix Verification Checklist

After making changes, verify:

### Functionality Preserved
- [ ] All functions in "Must Preserve" column still exist
- [ ] Hot-reload functions unchanged
- [ ] Factory functions return real implementations
- [ ] No new stub returns added

### Integration Verified
- [ ] Module initializes correctly
- [ ] Connected to render loop (if applicable)
- [ ] No bypassed abstractions

### Build Status
- [ ] Compiles without errors
- [ ] No new warnings about unused code
- [ ] All tests pass (if applicable)

---

## Sign-Off

- **Inventory Completed By**: [name/date]
- **Changes Made By**: [name/date]
- **Preservation Verified By**: [name/date]

---

## Change Log

| Date | Change | Functionality Impact |
|------|--------|---------------------|
| YYYY-MM-DD | Description | What was preserved/changed |
