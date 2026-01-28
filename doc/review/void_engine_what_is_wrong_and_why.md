# void_engine — What Was “Wrong” (and Why)

This document lists the key issues identified in the earlier design/harness approach **and the reason each issue matters** given your hard constraints:
- **All content (assets/scenes/plugins/widgets) is delivered via API deployments**
- **Hot swap / hot load is mandatory**
- **XR with layered creator content is the primary target**

This is not a criticism of the validation harness (that was useful). It explains what would break if the harness patterns became the application architecture.

---

## 1) Treating “init order” as a substitute for architecture

### What’s wrong
A single linear initialization list was trying to represent:
- dependency initialization
- frame execution order
- hot-reload boundaries
- feature layering

### Why it’s a problem
Those are different dimensions. Collapsing them makes:
- ordering brittle (small change forces reshuffle)
- hidden coupling (modules depend on “being earlier”)
- hot-reload unsafe (reload boundaries are unclear)
- layering hard (XR overlays want additive composition, not linear ownership)

**Fix direction**: separate
1) **Boot graph**
2) **Stage (frame) graph**
3) **Reload unit boundaries**

---

## 2) Scene graph implied as authoritative world state

### What’s wrong
“Scene” was positioned like it might own transforms/hierarchy (“scene graph, transforms, hierarchy”).

### Why it’s a problem (XR layering)
In XR, multiple creators/layers want to:
- attach overlays to the same physical object
- add remote avatars next to you
- contribute UI and logic independently

If a graph owns transforms/hierarchy, you need:
- merge rules
- priority rules
- conflict arbitration
- “who wins” ownership models

That creates a combinatorial mess.

**Fix direction**: ECS owns authoritative transforms and state; “graphs” are derived views/caches.

---

## 3) Two authoritative world states (ECS vs GameStateCore)

### What’s wrong
The harness demonstrated:
- ECS entities/components (Position/Health/etc.)
- a separate GameStateCore keyed by independent EntityId values

### Why it’s a problem
Without a single canonical identity and lifecycle:
- entity 1 in ECS may not be entity 1 in GameStateCore
- despawn/spawn desynchronizes stores
- hot-reload snapshots become inconsistent
- widgets bind to the “wrong truth”

**Fix direction** (choose one):
- ECS-first: gameplay state as ECS components + systems
- Hybrid: allowed only with one canonical EntityId and explicit spawn/despawn sync rules

---

## 4) Trigger callbacks as the primary integration model

### What’s wrong
Triggers were framed as:
- trigger volume detects overlap
- calls a callback that applies gameplay logic

### Why it’s a problem
Callback pointers:
- are unsafe across hot reload
- embed logic in the wrong layer
- prevent clean layering (multiple layers may want to respond)

**Fix direction**: triggers emit data events; plugin systems consume events.

---

## 5) Gameplay treated as engine modules rather than plugin deployments

### What’s wrong
Gameplay features (AI/combat/inventory) were placed as built-in engine modules/phases.

### Why it’s a problem in your platform
Your platform model is:
- creators build and deploy content
- engine consumes deployments via API
- logic must be hot-loadable and layered

Hardcoding gameplay as engine modules:
- fights the deployment model
- complicates versioning and migration
- makes layering and hot swap harder than necessary

**Fix direction**: engine provides ECS + scheduler + reload orchestration; gameplay arrives as plugins.

---

## 6) Hot-reload authority split across multiple places

### What’s wrong
Hot reload was “wired” through multiple paths:
- event bus subscriptions
- kernel reload manager callbacks
- per-service snapshot/restore logic

### Why it’s a problem
When reload authority is split:
- different subsystems reload at different times
- ordering guarantees are unclear
- “reload succeeded” can be true for one part and false for another
- state restoration becomes nondeterministic

**Fix direction**: Kernel orchestrates reload as the sole authority; publishes lifecycle events; others react.

---

## 7) Application loop started before the world existed (harness pattern)

### What’s wrong
The harness ran a partial render loop before:
- the world was loaded
- plugins/widgets were activated
- full stage scheduling existed

### Why it’s a problem
If carried into the app:
- you get multiple “mini loops”
- systems don’t have a consistent tick lifecycle
- hot reload timing becomes inconsistent
- XR pacing and frame stages cannot be enforced centrally

**Fix direction**: a single Runtime-owned loop drives all stages after full boot.

---

## 8) `main.cpp` doing too much (if kept as the real entrypoint)

### What’s wrong
The harness `main.cpp` mixed:
- bootstrap
- service definitions
- validation/demo logic
- loop logic
- shutdown logic

### Why it’s a problem
As the engine grows this becomes:
- untestable
- unmaintainable
- hard to make headless / editor / XR variants
- hard to define ownership and lifetime

**Fix direction**: production main is thin; Runtime owns lifecycle; Kernel owns stages/hot-reload.

---

## 9) Implicit assumption that some content is “built-in”

### What’s wrong
Some patterns implicitly assume “engine-provided built-in assets/content” (e.g., default gameplay behaviors).

### Why it’s a problem (your constraint)
You explicitly require:
- models/shaders/textures/etc. are delivered via API deployments
- logic and UI are also deployment-driven

Assuming built-ins creates:
- hidden dependencies
- mismatch between dev and deployed environments
- reduced portability between creators/projects

**Fix direction**: base engine provides only capability; content arrives via deployments and is always loadable/unloadable.

---

## 10) What this all means in XR

### What’s wrong (summary)
Non-ECS-authoritative graphs + callback triggers + built-in gameplay + split reload authority leads to:
- layering conflicts
- unsafe hot swap
- unclear ownership
- brittle composition

### Why it matters (XR reality)
XR experiences are:
- persistent worlds with transient overlays
- multiple concurrent contributors
- spatially anchored content that must rebind and migrate
- sensitive to frame pacing

The architecture must prefer:
- additive composition
- stable identities
- explicit stages
- deterministic reload orchestration

---

## Bottom-line diagnosis

The harness was fine for compilation/validation, but the “wrong” part was letting harness patterns imply the production architecture:
- linear phase lists standing in for system scheduling
- graph-first world ownership
- callback-first interactions
- gameplay-as-engine-modules
- split hot reload authority
- multiple truths for world state

With your constraints, the correct direction is:
- **ECS-first**
- **Scene == World**
- **Layers are patches**
- **Plugins provide systems**
- **Widgets are reactive views**
- **Kernel orchestrates reload**
- **Runtime owns lifecycle**
- **Everything arrives via API and is always loadable/unloadable**

