# Implementation Prompts

## Parallel Execution (RECOMMENDED)

### `parallel-implementation.md`

**How to run 9 sessions simultaneously** - one per module, no conflicts.

### Module-Specific Prompts (`modules/`)

| File | Module | Headers | Run Order |
|------|--------|---------|-----------|
| `modules/implement-core.md` | void_core | 8 | **FIRST** |
| `modules/implement-ir.md` | void_ir | 8 | Phase 2 |
| `modules/implement-presenter.md` | void_presenter | 12 | Phase 2 |
| `modules/implement-render.md` | void_render | 10 | Phase 2 |
| `modules/implement-asset.md` | void_asset | 7 | Phase 2 |
| `modules/implement-ecs.md` | void_ecs | 7 | Phase 2 |
| `modules/implement-compositor.md` | void_compositor | 7 | Phase 2 |
| `modules/implement-physics.md` | void_physics | 5 | Phase 2 |
| `modules/implement-services.md` | void_services | 4 | Phase 2 |

**Workflow**:
1. Run `implement-core.md` first (others depend on it)
2. Run remaining 8 modules in parallel
3. Merge CMakeLists.txt changes
4. Build and test

---

## Sequential Execution (Alternative)

### `implement-headers.md`

**Single prompt** covering all 68 headers. Use if you prefer one session at a time.

---

## Related Documents

| Document | Location | Purpose |
|----------|----------|---------|
| Implementation Analysis | `doc/IMPLEMENTATION_ANALYSIS.md` | Full technical specifications |
| Header Status | `doc/HEADER_INTEGRATION_GAPS.md` | Verified all headers complete |
| Master Checklist | `doc/MASTER_CHECKLIST.md` | Overall project status |
| Implementation Skill | `.claude/skills/implementation-gaps.md` | Detailed code templates |

## Workflow

1. Open `implement-gaps.md`
2. Copy everything below the separator line
3. Start new Claude session
4. Paste prompt
5. Follow the implementation order
