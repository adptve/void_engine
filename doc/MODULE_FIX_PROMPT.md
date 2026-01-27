# Module Fix Prompt Template

Copy this prompt into a fresh Claude Code terminal for each module.

---

## THE PROMPT

```
I need you to fix the [MODULE_NAME] module in void_engine using the preservation-first workflow.

CRITICAL RULES:
1. DO NOT run any builds until I explicitly say "BUILD NOW"
2. DO NOT comment out code to fix errors
3. DO NOT delete complex code - always keep the most advanced implementation
4. DO NOT chase errors - if something fails, STOP and report

YOUR TASK:
1. READ all files in the module (headers and cpp)
2. CREATE an inventory showing:
   - Every function with line counts
   - Which are stubs vs real implementations
   - Which have hot-reload support (snapshot/restore/dehydrate/rehydrate)
3. IDENTIFY the root cause of any issues (don't guess - prove it from code)
4. PROPOSE a surgical fix (one specific change)
5. WAIT for my approval before making any changes
6. SHOW before/after for any change you make
7. ONLY build when I say "BUILD NOW"

The workflow documentation is in: doc/BUILD_WORKFLOW.md
The inventory template is in: doc/templates/MODULE_INVENTORY_TEMPLATE.md

Start by reading the module files and creating the inventory. Do not make any changes yet.
```

---

## MODULE-SPECIFIC PROMPTS

### For render module:
```
I need you to fix the render module in void_engine using the preservation-first workflow.

KNOWN ISSUE: stub.cpp and gl_renderer.cpp have duplicate implementations.

CRITICAL RULES:
1. DO NOT run any builds until I explicitly say "BUILD NOW"
2. DO NOT comment out code to fix errors
3. DO NOT delete complex code - always keep the most advanced implementation
4. DO NOT chase errors - if something fails, STOP and report
5. gl_renderer.cpp has hot-reload support - this MUST be preserved

Start by reading both stub.cpp and gl_renderer.cpp. Create a side-by-side comparison of every duplicate function showing line counts. Identify which file has the more advanced implementation for each function.

Do not make any changes yet. Just report your findings.
```

### For xr module:
```
I need you to fix the xr module in void_engine using the preservation-first workflow.

KNOWN ISSUE: Namespace mismatch between header (void_xr) and implementation (void_engine::xr).

CRITICAL RULES:
1. DO NOT run any builds until I explicitly say "BUILD NOW"
2. DO NOT comment out code to fix errors
3. DO NOT delete complex code - always keep the most advanced implementation
4. DO NOT chase errors - if something fails, STOP and report

Start by reading the xr headers and cpp files. Document the namespace used in each file. Identify which namespace is correct and why.

Do not make any changes yet. Just report your findings.
```

### For compositor module:
```
I need you to fix the compositor module in void_engine using the preservation-first workflow.

KNOWN ISSUE: NullLayerCompositor::end_frame() references undefined 'layers' variable.

CRITICAL RULES:
1. DO NOT run any builds until I explicitly say "BUILD NOW"
2. DO NOT comment out code to fix errors
3. DO NOT delete complex code - always keep the most advanced implementation
4. DO NOT chase errors - if something fails, STOP and report

Start by reading layer_compositor.hpp. Find the NullLayerCompositor::end_frame() method. Understand what it's trying to do. Propose a fix that IMPLEMENTS the functionality, not stubs it out.

Do not make any changes yet. Just report your findings.
```

### For presenter module:
```
I need you to fix the presenter module in void_engine using the preservation-first workflow.

KNOWN ISSUE: Presenter is bypassed - main.cpp calls glfwSwapBuffers() directly instead of using IPresenter.

CRITICAL RULES:
1. DO NOT run any builds until I explicitly say "BUILD NOW"
2. DO NOT comment out code to fix errors
3. DO NOT delete complex code - always keep the most advanced implementation
4. DO NOT chase errors - if something fails, STOP and report

Start by reading the presenter module files and main.cpp. Document where presenter SHOULD be integrated. Identify what's currently bypassed.

Do not make any changes yet. Just report your findings.
```

---

## PROGRESS CHECK COMMANDS

Run these in a separate terminal to verify what's happening:

### Check if code was commented out (BAD):
```bash
# Count commented lines before
grep -c "^[[:space:]]*//" src/[module]/*.cpp > /tmp/comments_before.txt

# After changes, compare:
grep -c "^[[:space:]]*//" src/[module]/*.cpp > /tmp/comments_after.txt
diff /tmp/comments_before.txt /tmp/comments_after.txt

# If comment count increased significantly = RED FLAG
```

### Check line counts (should not decrease):
```bash
# Before:
wc -l src/[module]/*.cpp include/void_engine/[module]/*.hpp > /tmp/lines_before.txt

# After:
wc -l src/[module]/*.cpp include/void_engine/[module]/*.hpp > /tmp/lines_after.txt

diff /tmp/lines_before.txt /tmp/lines_after.txt
```

### Check hot-reload still exists:
```bash
grep -n "snapshot\|restore\|dehydrate\|rehydrate\|hot.reload\|HotReload" src/[module]/*.cpp
# This output should be the same or more after changes, never less
```

### Check for new stubs (BAD):
```bash
grep -n "return true;\|return false;\|return {};\|return nullptr;" src/[module]/*.cpp
# Compare before/after - should not increase
```

---

## VALIDATION CHECKLIST

Before saying "BUILD NOW", verify:

```markdown
## Module: [name]
## Date: [date]

### Pre-Change Baseline
- [ ] Recorded line counts: _____ lines total
- [ ] Recorded comment counts: _____ commented lines
- [ ] Recorded hot-reload patterns: _____ occurrences
- [ ] Recorded stub patterns: _____ occurrences

### Change Review
- [ ] AI showed before/after for each change
- [ ] No code was commented out
- [ ] No complex logic was "simplified"
- [ ] Hot-reload functions preserved
- [ ] Line count same or higher

### Post-Change Verification
- [ ] Line counts: _____ (should be >= baseline)
- [ ] Comment counts: _____ (should be <= baseline)
- [ ] Hot-reload patterns: _____ (should be >= baseline)
- [ ] Stub patterns: _____ (should be <= baseline)

### Approval
- [ ] APPROVED - Say "BUILD NOW"
- [ ] REJECTED - Explain what's wrong
```

---

## AFTER BUILD SUCCEEDS

```
The build passed. Now update the documentation:

1. Update doc/modules/[MODULE].md with:
   - Current status (compiles/works)
   - What was fixed
   - What was preserved
   - Any remaining issues

2. Update the mermaid diagram if architecture changed

3. Remove this module from the "needs fixing" list in doc/STUB_REMOVAL_PLAN.md

Show me the documentation updates before committing.
```

---

## IF BUILD FAILS

```
The build failed. DO NOT chase errors.

1. STOP making changes
2. Show me the first 5 unique errors (not all 50)
3. Analyze: Are they related? Same root cause?
4. Propose ONE fix for the root cause
5. Wait for my approval

Do not attempt multiple fixes. Do not comment anything out.
```

---

## MOVING TO NEXT MODULE

1. Close the terminal (clean slate)
2. Open fresh terminal
3. Run validation commands to record NEW baseline
4. Copy the appropriate prompt for next module
5. Start fresh

This prevents context pollution and error-chasing across modules.
