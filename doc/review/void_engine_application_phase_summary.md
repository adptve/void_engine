# void_engine — Application Phase Summary (ECS-first, Worlds, Hot-load Everything)

This consolidates the proposed changes/fixes from our discussion, aligned to your constraint that **all assets (models/shaders/textures/etc.) are delivered via API deployments** (nothing assumed built into the base).

---

## Definitions

### Scene == World
A **Scene** should mean a **World instance** that owns:
- one ECS world (entities + components)
- world spatial context (XR anchors / VR origin)
- active layer set (creator contributions)
- active plugin set (logic)
- active widget set (UI/HUD)

Switching XR experience → VR game is typically a **world switch** (different spatial assumptions and active layers). State crosses worlds only via explicit snapshot/restore.

### ECS-first, Graph-second
- **ECS is authoritative.**
- Any “scene graph” is **derived** (render submission view, UI layout tree, editor view), not the owner of transforms or hierarchy authority.

### Plugins
A **plugin** is a hot-loadable deployment unit that provides:
- component/state schemas (versioned; migratable)
- **systems** registered into frame stages
- event subscriptions / commands
- optional widget contributions

Plugins are the *delivery container*; systems are the ECS logic units.

### Widgets
A **widget** (or widget plugin) is a hot-loadable UI deployment unit that provides:
- widget types/templates
- UI systems (layout/binding/animation)
- bindings to world state via stable contracts (prefer ECS/world read interfaces)

---

## What needed fixing (root causes)

1) **Do not collapse three concerns into a single “init order”**
- Boot graph (dependencies)
- Frame stage graph (per-frame execution order)
- Hot-swap boundaries (reload units)

2) **Graph-owned transforms break layered XR**
Layered creator scenes (e.g., fridge overlay from one creator + remote avatar from another) require composition without ownership conflicts.
**Fix**: transforms and spatial relationships live in ECS; layers contribute components/systems.

3) **Trigger callbacks are hostile to hot reload**
Callback-first triggers couple gameplay code to trigger internals.
**Fix**: triggers emit data events (enter/exit/stay) into an event stream; plugins consume events. Callbacks can exist only as optional sugar on top.

4) **Gameplay “modules” should be plugins**
For your platform, AI/combat/inventory/etc. are typically **project/layer plugins**, not fixed engine modules.
**Fix**: the engine provides the plugin runtime + scheduler; plugins provide gameplay systems.

5) **Avoid two authoritative world states**
If ECS is the backbone, don’t maintain a separate gameplay state universe with mismatched IDs/lifetimes.
Pick one:
- ECS-first: gameplay state as components + systems (recommended)
- Hybrid: separate stores allowed only with one canonical EntityId and explicit spawn/despawn sync rules

6) **One hot-reload orchestrator**
Do not split hot-reload authority across multiple managers.
**Fix**: Kernel orchestrates reload (snapshot → unload → reload → restore → post) and publishes events; everything else subscribes.

7) **Validation harness vs application entry**
Your large `main.cpp` is a good validation harness, but should not become the production application entrypoint.

---

## Proposed application flow (high level)

### Boot (one-time)
1. Foundation: memory/core/structures/math
2. Infrastructure: event bus + services + **kernel**
3. API clients: asset/deployment clients, watchers, authentication/session
4. Platform: presenter/render backend/compositor (skip if headless)
5. I/O: input + audio
6. Simulation base: ECS + scheduler, physics, triggers (event emission)
7. World load: request world definition via API, instantiate into ECS
8. Plugin runtime: load/activate plugin deployments, register systems into stages
9. Widget runtime: load/activate widget deployments, register UI systems/bindings
10. Start main loop

### Frame (repeat)
- Input
- Hot-reload poll (deployments/assets/plugins/widgets/layers)
- Event dispatch
- Update (plugins/scripting/gameplay)
- FixedUpdate (physics)
- PostFixed (emit/consume trigger & physics events)
- RenderPrepare
- Render + Present
- UI update/render
- Audio update
- Streaming / API sync

---

## Mermaid diagram (boot + frame stages)

~~~mermaid
flowchart TD
  A[main] --> B[Runtime startup]
  B --> C[Kernel init: stages + hot-reload]
  C --> D[Init foundation]
  D --> E[Init infra: event bus + services]
  E --> F[Init API clients: assets/deployments/watchers]
  F --> G[Init platform: presenter/render/compositor]
  G --> H[Init IO: input/audio]
  H --> I[Init simulation base: ECS + physics + triggers(events)]
  I --> J[Load World via API -> instantiate ECS]
  J --> K[Load Plugins via API -> register systems into stages]
  K --> L[Load Widgets via API -> register UI systems/bindings]
  L --> M[Main loop]

  M --> N{Exit?}
  N -- no --> S[Frame stages]
  S --> S1[Input]
  S1 --> S2[HotReloadPoll]
  S2 --> S3[EventDispatch]
  S3 --> S4[Update]
  S4 --> S5[FixedUpdate]
  S5 --> S6[PostFixed]
  S6 --> S7[RenderPrepare]
  S7 --> S8[Render+Present]
  S8 --> S9[UI]
  S9 --> S10[Audio]
  S10 --> S11[Streaming/API sync]
  S11 --> N

  N -- yes --> Z[Shutdown reverse order]
~~~

---

## main.cpp, Runtime, Kernel (conceptual expectations)

### main.cpp
- Only: parse CLI/manifest pointer → create Runtime → run.
- Keep your current large file as a separate validation executable or `--validate` mode.

### Runtime responsibilities
- application lifecycle (startup / run loop / shutdown)
- world creation/unload/switch (scene == world)
- driving the frame stage pipeline each tick
- mode selection: headless vs windowed vs XR (extensions later)
- coordinating API connectivity and deployment updates

### Kernel responsibilities
- stage scheduler (system registration + execution per stage)
- hot-reload orchestration for all hot-load units (world layers, plugins, widgets)
- dependency correctness checks / ordering invariants
- publishing reload/state-change events to the event bus

---

## XR expectations (layered creators)

- A stable **World** runs while creator layers can be added/removed dynamically.
- Layers contribute entities/components/systems/widgets via API deployments.
- Remote presence is “just another layer” (network replication systems + avatar entities + voice entities).
- ECS-first allows independent authoring without graph ownership conflicts.

---

## Action list (application phase, extensions skipped)

1) Build Runtime + Kernel (thin main, real loop in Runtime).
2) Formalize frame stages (stop encoding runtime order via init order).
3) Convert triggers to event-emitting core.
4) Unify entity identity/lifetimes across ECS and any external stores.
5) Make world loading purely “API → ECS instantiation + layer activation”.
6) Treat gameplay as plugins and UI as widget deployments; both register systems into stages.
7) Make Kernel the single hot-reload orchestrator.

