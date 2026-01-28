# void_engine — Comprehensive Application Architecture (ECS‑First, Worlds, XR‑Native)

This document is the **comprehensive** version of the application‑phase architecture for **void_engine**, consolidating all guidance from our discussion and aligning strictly with your constraints:

- **Everything is loadable via API** (assets, scenes, plugins, widgets, logic)
- **Hot‑swap / hot‑reload is a first‑class requirement**
- **XR with layered creator content is the primary target**
- **ECS is authoritative**
- **Scenes == Worlds**

This intentionally avoids code samples and focuses on **structure, responsibility, and flow**.

---

## 1. Core Mental Model (this is the foundation)

### 1.1 Scene == World (non‑negotiable)

In void_engine:

> A **Scene is a World instance**, not a graph, not a level file, not a hierarchy.

A **World** owns:
- exactly **one ECS world**
- spatial context (XR anchors OR VR origin, never both)
- active **layers** (creator contributions)
- active **plugins** (gameplay/logic systems)
- active **widgets** (UI/HUD)
- simulation state (physics, networking context, etc.)

Switching from:
- XR → VR
- world‑scan → game
- social space → combat arena  

…is a **world switch**.

State only crosses worlds via **explicit snapshot / restore**, never by sharing live objects.

---

## 2. ECS‑First, Graph‑Second (why this matters for XR)

### 2.1 ECS is authoritative
- Entity existence
- Transforms
- Ownership
- Gameplay state
- XR anchoring
- Network identity

All live **in ECS**.

### 2.2 Graphs are derived views
“Scene graph”, “UI tree”, “render graph”, “animation graph” are:
- **derived**
- **cached**
- **throw‑away**
- **rebuildable**

They must never own authoritative state.

This is what enables:
- multiple creators layering content onto the same physical object
- hot‑reload without dangling references
- XR anchor rebinding
- deterministic state migration

---

## 3. Layers (creator content model)

A **layer** is a loadable contribution to a world.

A layer may:
- spawn entities
- attach components to existing entities
- register systems (via plugins)
- register widgets
- bind to events or data streams

A layer must **never**:
- assume ownership of the world
- assume transform hierarchy control
- assume exclusive access to entities it didn’t create

Think of layers as **patches**, not sub‑worlds.

---

## 4. Plugins (what they really are)

### 4.1 Plugin ≠ System

A **plugin** is a **deployment unit**.

A plugin can provide:
- component schemas (versioned)
- systems (ECS logic)
- event handlers
- commands
- bindings
- optional widget contributions

A **system** is just logic registered into a frame stage.

### 4.2 Why gameplay must be plugins
AI, combat, inventory, interaction logic:
- are project‑specific
- are creator‑authored
- must be hot‑reloadable
- must be layered

Therefore:
> Gameplay is **not an engine module**, it is plugin content.

The engine provides:
- ECS
- scheduler
- hot‑reload orchestration

Plugins provide:
- behavior.

---

## 5. Widgets (UI in an XR‑first engine)

### 5.1 Widgets are not game objects
Widgets are:
- view‑models
- reactive
- bound to world state
- replaceable at runtime

They should never:
- pull raw pointers to game state
- reach into ECS internals directly
- depend on gameplay object lifetimes

### 5.2 Widget bindings
Widgets bind to:
- ECS data (recommended)
- or stable world data interfaces

In XR:
- widgets may be world‑space
- screen‑space
- gaze‑triggered
- ephemeral overlays

All are handled the same way: **widget state + bindings + systems**.

---

## 6. Triggers and events (critical fix)

### 6.1 Why callbacks are wrong
Callbacks:
- break hot‑reload
- capture invalid code pointers
- create hidden dependencies
- block layering

### 6.2 Correct model
Triggers:
- detect spatial conditions
- emit **data events**

Plugins:
- subscribe to events
- decide what to do

This decouples:
- detection
- logic
- reload boundaries

---

## 7. Hot‑Reload: one authority

### 7.1 Kernel owns hot‑reload
Hot‑reload must be **orchestrated**, not opportunistic.

Kernel responsibilities:
- snapshot participants
- unload in dependency order
- reload deployments
- restore compatible state
- emit reload lifecycle events

Nothing else coordinates reloads.
Everything else **reacts**.

---

## 8. Application responsibilities (Runtime vs Kernel)

### 8.1 main.cpp (production)
`main` does **almost nothing**:
- parse CLI
- load config / manifest pointer
- create Runtime
- call `run()`

Your current giant main is correct as a **validation harness**, not as the app.

---

### 8.2 Runtime (application owner)

Runtime owns:
- process lifecycle
- world creation / destruction
- world switching
- frame loop
- mode selection (headless / windowed / XR)
- API connectivity
- deployment polling

Runtime does **not**:
- contain gameplay logic
- schedule systems directly
- manage hot‑reload details

---

### 8.3 Kernel (orchestrator)

Kernel owns:
- frame **stage scheduler**
- system registration
- execution order
- hot‑reload orchestration
- dependency enforcement
- lifecycle events

Kernel is not “the engine loop”.
It is the **execution authority**.

---

## 9. Frame model (what actually runs)

### 9.1 Stages (explicit, not implicit)

Typical stages:
- Input
- HotReloadPoll
- EventDispatch
- Update
- FixedUpdate
- PostFixed
- RenderPrepare
- Render
- UI
- Audio
- Streaming / API sync

Plugins register systems into stages.
Order is explicit and inspectable.

---

## 10. World lifecycle

World creation:
- request world definition via API
- stream assets
- instantiate ECS entities
- activate layers
- activate plugins
- activate widgets

World destruction:
- snapshot if needed
- deactivate layers
- unload plugins/widgets
- destroy ECS world

---

## 11. XR‑specific expectations

XR requires:
- long‑lived worlds
- transient layers
- late‑bound anchors
- multiple coordinate spaces
- remote presence as just another layer

This architecture supports:
- walking up to a fridge and seeing data from creator A
- seeing a remote avatar from creator B
- switching to a VR game without restarting the engine

All without:
- graph merging
- pointer sharing
- mode flags everywhere

---

## 12. Mermaid overview (boot + runtime)

~~~mermaid
flowchart TD
  A[main] --> B[Runtime startup]
  B --> C[Kernel init]
  C --> D[Foundation]
  D --> E[Infrastructure]
  E --> F[API connectivity]
  F --> G[Platform]
  G --> H[IO]
  H --> I[Simulation base]
  I --> J[Load World]
  J --> K[Load Plugins]
  K --> L[Load Widgets]
  L --> M[Main Loop]

  M --> N{Exit?}
  N -- no --> O[Frame Stages]
  O --> O1[Input]
  O1 --> O2[HotReloadPoll]
  O2 --> O3[EventDispatch]
  O3 --> O4[Update]
  O4 --> O5[FixedUpdate]
  O5 --> O6[PostFixed]
  O6 --> O7[RenderPrepare]
  O7 --> O8[Render]
  O8 --> O9[UI]
  O9 --> O10[Audio]
  O10 --> O11[Streaming]
  O11 --> N

  N -- yes --> Z[Shutdown]
~~~

---

## 13. Final invariants (do not violate these)

- ECS is authoritative
- Scene == World
- Plugins contain systems
- Widgets are reactive views
- Layers are patches, not owners
- Kernel orchestrates reload
- Runtime owns lifecycle
- Everything is loadable via API

If you keep these invariants intact, the engine will scale cleanly for XR, layering, and creator‑driven worlds.

