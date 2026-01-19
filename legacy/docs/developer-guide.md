# Void GUI v2 / Metaverse OS - App Developer Guide

> **Build apps for the Metaverse OS platform**
>
> This guide covers everything you need to know to create, test, and publish apps.

---

## Table of Contents

1. [Getting Started](#1-getting-started)
2. [App Package Format](#2-app-package-format-mvp)
3. [Manifest Reference](#3-manifest-reference)
4. [Layer System](#4-layer-system)
5. [Scripting](#5-scripting)
6. [IR Patch System](#6-ir-patch-system)
7. [Restrictions & Limitations](#7-restrictions-and-limitations)
8. [Security Model](#8-security-model)
9. [Testing & Debugging](#9-testing-and-debugging)
10. [Publishing](#10-publishing)

---

## 1. Getting Started

### Prerequisites

To develop apps for the Void GUI v2 / Metaverse OS platform, you will need:

- **VoidScript Knowledge**: The built-in scripting language for app logic
- **WASM Toolchain** (optional): For advanced behavior plugins (Rust, C, or AssemblyScript to WASM)
- **Text Editor**: Any editor that supports TOML and VoidScript syntax highlighting
- **Package Tools**: The `void-pkg` CLI tool for building and packaging apps

### Development Environment Setup

```bash
# 1. Install the Void SDK (includes void-pkg tool)
# 2. Create a new project
void-pkg init my-awesome-app

# 3. Edit manifest.toml with your app configuration
# 4. Write your VoidScript code in scripts/main.vs
# 5. Test locally
void-pkg run

# 6. Package for distribution
void-pkg build
```

### Project Structure

```
myapp/
├── manifest.toml        # Required: App manifest
├── icon.png             # Optional: App icon (128x128 or 256x256 PNG)
├── scripts/
│   ├── main.vs          # Required: Entry point script
│   └── utils.vs         # Optional: Additional scripts
├── assets/
│   ├── models/          # 3D models (.glb, .gltf)
│   ├── textures/        # Texture files (.png, .jpg)
│   └── audio/           # Audio files (.wav, .ogg)
└── wasm/                # Optional: WASM behavior plugins
    └── behavior.wasm
```

---

## 2. App Package Format (.mvp)

The `.mvp` (Metaverse Package) format is a ZIP-based archive format for distributing apps.

### Package Structure

```
myapp.mvp (ZIP archive)
├── manifest.toml      # App manifest (REQUIRED)
├── icon.png           # App icon (optional)
├── assets/            # Asset files
│   ├── models/
│   ├── textures/
│   └── audio/
├── scripts/           # VoidScript files
│   └── main.vs        # Entry point (REQUIRED)
├── wasm/              # WASM modules (optional)
│   └── logic.wasm
└── signature          # Package signature (optional)
```

### Package Constraints

| Property | Limit |
|----------|-------|
| Maximum package size | 100 MB |
| Package format version | 1 |
| File extension | `.mvp` |
| Magic bytes | `MVP\x01` |

### Compression Options

| Method | Description | Use Case |
|--------|-------------|----------|
| None | No compression | Pre-compressed assets |
| Deflate | ZIP standard (default, level 6) | General use |
| LZ4 | Fast compression | Large assets, quick loading |

---

## 3. Manifest Reference

The `manifest.toml` file defines your app's metadata, configuration, permissions, and resource requirements.

### Complete Example

```toml
[package]
name = "my-awesome-app"
display_name = "My Awesome App"
version = "1.0.0"
description = "An example app for Metaverse OS"
author = "Your Name"
email = "you@example.com"
homepage = "https://example.com/my-app"
repository = "https://github.com/username/my-app"
license = "MIT"
keywords = ["game", "puzzle", "3d"]
categories = ["games", "entertainment"]

[app]
app_type = "app"           # app, service, widget, game, tool
entry = "scripts/main.vs"  # Entry point script

[[app.layers]]
name = "content"
type = "content"
priority = 0

[[app.layers]]
name = "ui"
type = "overlay"
priority = 100
blend = "normal"

[app.permissions]
network = false
filesystem = false
scripts = true
cross_app_read = false
camera = false
microphone = false
location = false

[app.resources]
max_entities = 5000
max_memory = 134217728    # 128 MB in bytes
max_layers = 4
max_cpu_ms = 8.0

[[dependencies]]
name = "void-ui-toolkit"
version = "^1.0.0"
optional = false

[assets]
include = ["assets/"]
exclude = ["assets/dev/"]
compression = 6

[scripts]
language = "voidscript"
wasm_modules = ["wasm/logic.wasm"]

[platform]
min_version = "1.0.0"
required_features = ["webgpu"]
platforms = ["windows", "linux", "macos"]

[platform.xr]
vr = true
ar = false
mr = false
hand_tracking = false
passthrough = false
```

### Section Reference

#### [package] - Package Metadata

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `name` | String | **Yes** | - | Unique package identifier (lowercase, hyphens only) |
| `display_name` | String | No | name | Human-readable display name |
| `version` | String | **Yes** | - | Semantic version (e.g., "1.0.0") |
| `description` | String | No | None | Brief description of the app |
| `author` | String | No | None | Author name |
| `email` | String | No | None | Author contact email |
| `homepage` | String | No | None | Homepage URL |
| `repository` | String | No | None | Source code repository URL |
| `license` | String | No | None | SPDX license identifier |
| `keywords` | String[] | No | [] | Keywords for discovery |
| `categories` | String[] | No | [] | Category classifications |

#### [app] - App Configuration

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `app_type` | String | No | "app" | Type: `app`, `service`, `widget`, `game`, `tool` |
| `entry` | String | No | "scripts/main.vs" | Entry point script path |

#### [[app.layers]] - Layer Definitions (Array)

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `name` | String | **Yes** | - | Layer identifier |
| `type` | String | **Yes** | - | Type: `content`, `effect`, `overlay`, `portal` |
| `priority` | Integer | No | 0 | Z-order (higher = rendered on top) |
| `blend` | String | No | "normal" | Blend mode (see Layer System) |

#### [app.permissions] - Requested Permissions

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `network` | Boolean | false | Access network/internet |
| `filesystem` | Boolean | false | Access local filesystem |
| `scripts` | Boolean | true | Execute scripts |
| `cross_app_read` | Boolean | false | Read other apps' entities |
| `camera` | Boolean | false | Access camera/webcam |
| `microphone` | Boolean | false | Access microphone |
| `location` | Boolean | false | Access location services |

#### [app.resources] - Resource Limits

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `max_entities` | Integer | 10,000 | Maximum entities this app can create |
| `max_memory` | Integer | 256 MB | Maximum memory usage (bytes) |
| `max_layers` | Integer | 8 | Maximum layers this app can request |
| `max_cpu_ms` | Float | 16.0 | Maximum CPU time per frame (ms) |

#### [[dependencies]] - Dependencies (Array)

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `name` | String | **Yes** | - | Dependency package name |
| `version` | String | **Yes** | - | Semver version requirement |
| `optional` | Boolean | No | false | Whether dependency is optional |

#### [platform] - Platform Requirements

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `min_version` | String | None | Minimum OS version required |
| `required_features` | String[] | [] | Required platform features |
| `platforms` | String[] | [] | Supported platforms |

#### [platform.xr] - XR Support

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `vr` | Boolean | false | VR headset support |
| `ar` | Boolean | false | AR/passthrough support |
| `mr` | Boolean | false | Mixed reality support |
| `hand_tracking` | Boolean | false | Requires hand tracking |
| `passthrough` | Boolean | false | Requires passthrough |

---

## 4. Layer System

Layers are the primary rendering isolation mechanism. Each app renders to its own layers, and the kernel composites them together.

### Layer Types

| Type | Description | Typical Use | Depth Buffer |
|------|-------------|-------------|--------------|
| `content` | Standard 3D content | Main world geometry, characters | Yes |
| `effect` | Post-processing effects | Bloom, color grading, blur | No |
| `overlay` | 2D UI overlays | Menus, HUD, text | No |
| `portal` | Render another view | Mirrors, portals, PiP | Yes |

### Priority and Z-Ordering

Layers are rendered in priority order (lowest first, highest last):

```toml
# Background layer (rendered first)
[[app.layers]]
name = "background"
type = "content"
priority = -100

# Main content (rendered in middle)
[[app.layers]]
name = "world"
type = "content"
priority = 0

# Post-process effects
[[app.layers]]
name = "effects"
type = "effect"
priority = 50

# UI overlay (rendered last, on top)
[[app.layers]]
name = "ui"
type = "overlay"
priority = 100
```

### Blend Modes

| Mode | Description | Formula |
|------|-------------|---------|
| `normal` | Standard alpha blending | `dst = src * alpha + dst * (1-alpha)` |
| `additive` | Add colors together | `dst = src + dst` |
| `multiply` | Darken by multiplication | `dst = src * dst` |
| `screen` | Lighten | `dst = 1 - (1-src) * (1-dst)` |
| `replace` | No blending, full replace | `dst = src` |

### Best Practices

1. **Minimize layer count**: Each layer has overhead. Use 2-4 layers for most apps.
2. **Use appropriate types**: Content for 3D, Overlay for 2D UI.
3. **Priority gaps**: Leave gaps (0, 100, 200) for easier insertion later.
4. **Effect layers**: Use sparingly, they require full-screen passes.

---

## 5. Scripting

Apps can use VoidScript for logic and optionally WASM modules for performance-critical code.

### VoidScript Basics

VoidScript is a simple, JavaScript-like scripting language.

#### Variables

```javascript
let x = 10;
let name = "Player";
let position = [0.0, 1.0, 0.0];
let config = { speed: 5.0, health: 100 };
```

#### Data Types

| Type | Example | Description |
|------|---------|-------------|
| Integer | `42`, `-10` | 64-bit signed integer |
| Float | `3.14`, `-0.5` | 64-bit floating point |
| Boolean | `true`, `false` | Boolean value |
| String | `"hello"` | UTF-8 string |
| Array | `[1, 2, 3]` | Dynamic array |
| Object | `{ x: 1, y: 2 }` | Key-value map |
| Null | `null` | Null/none value |

#### Functions

```javascript
fn add(a, b) {
    return a + b;
}

// Lambda functions
let double = |x| x * 2;
```

#### Control Flow

```javascript
// If/else
if score > 100 {
    print("High score!");
} else if score > 50 {
    print("Good score");
} else {
    print("Try again");
}

// While loop
let i = 0;
while i < 10 {
    print(i);
    i = i + 1;
}

// For loop
for item in items {
    print(item);
}
```

#### Built-in Functions

```javascript
print("Hello");              // Output to console
len([1, 2, 3]);              // Get length: 3
type(42);                    // Get type: "int"
str(42);                     // Convert to string: "42"
int("42");                   // Parse integer: 42
float("3.14");               // Parse float: 3.14
```

### Entry Points and Lifecycle Hooks

Your main script (`scripts/main.vs`) should define these lifecycle functions:

```javascript
// Called once when app starts
fn on_init() {
    print("App initialized");
}

// Called every frame
fn on_update(dt) {
    // dt = delta time in seconds
}

// Called when app receives focus
fn on_focus() { }

// Called when app loses focus
fn on_blur() { }

// Called when app is being unloaded
fn on_shutdown() {
    print("Cleaning up");
}
```

### WASM Module Integration

For performance-critical code, you can write WASM modules:

#### Exported Functions (from your WASM)

```rust
#[no_mangle]
pub extern "C" fn on_spawn(entity_id: u32) { }

#[no_mangle]
pub extern "C" fn on_update(entity_id: u32, delta_time: f32) { }

#[no_mangle]
pub extern "C" fn on_destroy(entity_id: u32) { }
```

#### Host Functions (available to WASM)

```rust
extern "C" fn spawn_entity(prefab_ptr: i32, prefab_len: i32, x: f32, y: f32, z: f32) -> i32;
extern "C" fn log(level: i32, msg_ptr: i32, msg_len: i32);
extern "C" fn get_time() -> f64;
extern "C" fn get_delta_time() -> f32;
extern "C" fn set_position(entity_id: i32, x: f32, y: f32, z: f32);
```

---

## 6. IR Patch System

Apps communicate with the kernel exclusively through **patches**. Patches are declarative operations that describe what changes should be made to the world.

### Why Patches?

- **Atomic**: Changes are applied all-or-nothing
- **Validated**: Kernel checks permissions before applying
- **Rollback-able**: Can be undone if needed
- **Auditable**: All changes are logged

### Patch Types

#### Entity Patches

```javascript
// Create entity
emit_patch({
    type: "entity",
    entity: { namespace: my_namespace, local_id: 1 },
    op: "create",
    archetype: "Player",
    components: {
        "Transform": { position: [0, 0, 0], rotation: [0, 0, 0, 1], scale: [1, 1, 1] },
        "Renderable": { mesh: "player.glb", material: "default" }
    }
});

// Destroy entity
emit_patch({
    type: "entity",
    entity: { namespace: my_namespace, local_id: 1 },
    op: "destroy"
});

// Enable/disable entity
emit_patch({
    type: "entity",
    entity: my_entity,
    op: "enable"  // or "disable"
});

// Set parent (for hierarchy)
emit_patch({
    type: "entity",
    entity: child_entity,
    op: "set_parent",
    parent: parent_entity  // or null to unparent
});
```

#### Component Patches

```javascript
// Set component (add or replace)
emit_patch({
    type: "component",
    entity: my_entity,
    component: "Transform",
    op: "set",
    data: {
        position: [10, 0, 5],
        rotation: [0, 0, 0, 1],
        scale: [1, 1, 1]
    }
});

// Update specific fields only
emit_patch({
    type: "component",
    entity: my_entity,
    component: "Transform",
    op: "update",
    fields: {
        position: [10, 0, 5]  // Only update position
    }
});

// Remove component
emit_patch({
    type: "component",
    entity: my_entity,
    component: "Health",
    op: "remove"
});
```

#### Layer Patches

```javascript
// Create layer at runtime
emit_patch({
    type: "layer",
    layer_id: "my_effect_layer",
    op: "create",
    layer_type: "effect",
    priority: 50
});

// Update layer properties
emit_patch({
    type: "layer",
    layer_id: "my_effect_layer",
    op: "update",
    visible: false
});
```

#### Asset Patches

```javascript
// Load asset
emit_patch({
    type: "asset",
    asset_id: "player_model",
    op: "load",
    path: "assets/models/player.glb",
    asset_type: "mesh"
});

// Unload asset
emit_patch({
    type: "asset",
    asset_id: "player_model",
    op: "unload"
});
```

### Transaction Building

Group patches into transactions for atomic application:

```javascript
let tx = begin_transaction()
    .description("Spawn player with inventory")
    .patch(create_entity_patch)
    .patch(add_transform_patch)
    .patch(add_renderable_patch)
    .build();

submit_transaction(tx);
```

### What Apps CAN Do

| Operation | Allowed |
|-----------|---------|
| Create entities in own namespace | ✅ |
| Modify own entities' components | ✅ |
| Destroy own entities | ✅ |
| Create/manage own layers | ✅ |
| Load assets from own package | ✅ |
| Read other apps' entities (with permission) | ✅ |

### What Apps CANNOT Do

| Operation | Blocked |
|-----------|---------|
| Modify entities in other namespaces | ❌ |
| Create entities in other namespaces | ❌ |
| Access filesystem (without permission) | ❌ |
| Access network (without permission) | ❌ |
| Exceed resource budgets | ❌ |
| Bypass capability system | ❌ |
| Execute system calls | ❌ |
| Access kernel internals | ❌ |

---

## 7. Restrictions and Limitations

### Resource Budget Limits

| Resource | Default | Minimum | Maximum | Description |
|----------|---------|---------|---------|-------------|
| Memory | 256 MB | 32 MB | 1 GB | Total RAM usage |
| GPU Memory | 512 MB | 64 MB | 2 GB | VRAM for textures/buffers |
| Entities | 10,000 | 1,000 | 100,000 | Maximum entity count |
| Layers | 8 | 2 | 16 | Maximum render layers |
| Assets | 1,000 | 100 | 10,000 | Maximum loaded assets |
| Frame Time | 16 ms | 8 ms | 32 ms | Max CPU time per frame |
| Patches/Frame | 1,000 | 100 | 10,000 | Maximum patches per frame |
| Draw Calls | 1,000 | 100 | 5,000 | Maximum draw calls |

### Capability Restrictions

**Default Capabilities** (automatically granted):

| Capability | Limit |
|------------|-------|
| Create Entities | 10,000 max |
| Destroy Entities | Unlimited |
| Modify Components | All types |
| Create Layers | 8 max |
| Load Assets | Package paths only |

**Requires Permission Request**:

| Capability | Manifest Field |
|------------|----------------|
| Network Access | `permissions.network = true` |
| Filesystem Access | `permissions.filesystem = true` |
| Cross-App Read | `permissions.cross_app_read = true` |
| Camera | `permissions.camera = true` |
| Microphone | `permissions.microphone = true` |
| Location | `permissions.location = true` |

**Never Granted to Apps**:

| Capability | Reason |
|------------|--------|
| HotSwap | Kernel-only |
| ManageCapabilities | Kernel-only |
| KernelAdmin | Kernel-only |

### Crash Limits

- **Maximum crashes before termination**: 3
- **Backoff between restarts**: 100ms → 200ms → 400ms (exponential)
- **Crash window**: 60 seconds

---

## 8. Security Model

### Namespace Isolation

Each app operates in complete isolation:

```
┌─────────────────────────────────────┐
│ App: my-game (namespace: ns_12345)  │
├─────────────────────────────────────┤
│ Entities: ns_12345:1, ns_12345:2... │
│ Layers: ns_12345:content, :ui       │
│ Components: ns_12345:CustomComp     │
└─────────────────────────────────────┘
         │
         │ Cannot access
         ▼
┌─────────────────────────────────────┐
│ App: other-app (namespace: ns_67890)│
└─────────────────────────────────────┘
```

### Capability Properties

Based on seL4 microkernel design:

1. **Unforgeable**: Capabilities cannot be guessed or manufactured
2. **Explicit**: All permissions must be explicitly granted
3. **Revocable**: Capabilities can be revoked at any time
4. **Fine-grained**: Specific capabilities for specific operations
5. **Auditable**: All capability checks are logged

### Crash Containment

- App panics are caught via `catch_unwind`
- Kernel never crashes due to app failure
- All app resources cleaned up on crash
- Supervision tree handles automatic restart

---

## 9. Testing and Debugging

### Local Testing

```bash
# Run app in development mode
void-pkg run

# Run with verbose logging
void-pkg run --verbose

# Run with specific permissions for testing
void-pkg run --allow-network --allow-filesystem

# Run with resource limits
void-pkg run --max-entities 1000 --max-memory 64MB
```

### Debugging Output

```javascript
// Console output
print("Debug: player position = " + str(position));

// Get metrics
let metrics = get_metrics();
print("Frame time: " + str(metrics.frame_time_ms) + "ms");
print("Entity count: " + str(metrics.entity_count));
print("Patches this frame: " + str(metrics.patches_this_frame));
```

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `PermissionDenied` | Missing capability | Add permission to manifest |
| `QuotaExceeded` | Resource limit hit | Increase limits or optimize |
| `UnknownNamespace` | Invalid entity reference | Check entity exists in your namespace |
| `TooManyPatches` | Exceeded patches/frame | Batch operations, spread over frames |
| `TooManyLayers` | Layer limit reached | Remove unused layers |
| `EntryNotFound` | Missing file in package | Verify file exists and path is correct |
| `InvalidManifest` | Malformed manifest.toml | Validate TOML syntax |

---

## 10. Publishing

### Package Building

```bash
# Build package from project directory
void-pkg build

# Build with optimizations
void-pkg build --release

# Build with specific output name
void-pkg build --output myapp-1.0.0.mvp

# Validate package without building
void-pkg validate
```

### Package Signing

```bash
# Generate signing key (one-time)
void-pkg keygen --output mykey.pem

# Sign package
void-pkg sign myapp.mvp --key mykey.pem

# Verify signature
void-pkg verify myapp.mvp
```

### Submission Process

> **Note**: The official app marketplace is under development. Apps will eventually be consumed via an external service.

The submission process will include:

1. **Automated Validation**
   - Valid manifest format
   - Required files present
   - Resource limits within platform maximums
   - No malicious patterns detected

2. **Permission Review**
   - Apps requesting sensitive permissions (network, filesystem, camera) undergo additional review
   - Justification required for each permission

3. **Content Review**
   - Content guidelines compliance
   - Quality standards

4. **Publication**
   - Package signed with store key
   - Listed in marketplace
   - Available for installation

### Version Requirements

- Semantic versioning required (MAJOR.MINOR.PATCH)
- Version must increase for updates
- Breaking changes require MAJOR bump
- Package filename: `<name>-<version>.mvp`

---

## Appendix: Quick Reference

### Patch Operations

| Patch Type | Operations |
|------------|------------|
| Entity | `create`, `destroy`, `enable`, `disable`, `set_parent`, `add_tag`, `remove_tag` |
| Component | `set`, `update`, `remove` |
| Layer | `create`, `update`, `destroy` |
| Asset | `load`, `unload`, `update` |

### Lifecycle Hooks

| Hook | When Called |
|------|-------------|
| `on_init()` | App starts |
| `on_update(dt)` | Every frame |
| `on_focus()` | App receives focus |
| `on_blur()` | App loses focus |
| `on_shutdown()` | App unloading |

### Layer Types

| Type | Use For |
|------|---------|
| `content` | 3D world, characters, objects |
| `effect` | Post-processing, bloom, blur |
| `overlay` | 2D UI, HUD, menus |
| `portal` | Mirrors, portals, picture-in-picture |

---

## Getting Help

- **Documentation**: `/docs/` directory
- **Examples**: `/examples/` directory (coming soon)
- **Issues**: Report bugs via the project issue tracker

---

*Last updated: 2026-01-05*
