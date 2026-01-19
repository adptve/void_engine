# Spatial Queries

Spatial data structures for efficient collision and queries.

## Scene-Level Spatial Config

```toml
[spatial]
structure = "bvh"              # Spatial structure type
auto_rebuild = true            # Rebuild on changes
rebuild_threshold = 0.3        # Fraction of changes triggering rebuild
```

### Structure Types

| Type | Description | Best For |
|------|-------------|----------|
| `bvh` | Bounding Volume Hierarchy | General use, ray queries |
| `octree` | Spatial octree | Uniform distribution |
| `grid` | Uniform grid | Dense, regular scenes |

## BVH Configuration

```toml
[spatial.bvh]
max_leaf_size = 4              # Max objects per leaf node
build_quality = "medium"       # Build algorithm quality
```

### Build Quality

| Quality | Build Time | Query Time |
|---------|------------|------------|
| `fast` | Fastest | Slower |
| `medium` | Balanced | Balanced |
| `high` | Slowest | Fastest |

## Query Configuration

```toml
[spatial.queries]
frustum_culling = true         # Cull objects outside view
occlusion_culling = false      # Cull occluded objects
max_query_results = 500        # Maximum results per query
```

## App-Level Spatial Config

Configure in `manifest.toml`:

```toml
[app.spatial]
structure = "bvh"
rebuild_threshold = 0.3
max_depth = 32                 # Max tree depth
min_objects_per_node = 2       # Min objects before splitting
```

## Spatial Presets

### General Purpose

```toml
[spatial]
structure = "bvh"
auto_rebuild = true
rebuild_threshold = 0.3

[spatial.bvh]
max_leaf_size = 4
build_quality = "medium"

[spatial.queries]
frustum_culling = true
occlusion_culling = false
max_query_results = 500
```

### High Performance (Static Scene)

```toml
[spatial]
structure = "bvh"
auto_rebuild = false           # Manual rebuild only

[spatial.bvh]
max_leaf_size = 2
build_quality = "high"         # Optimal structure

[spatial.queries]
frustum_culling = true
occlusion_culling = true
max_query_results = 1000
```

### Dynamic Scene

```toml
[spatial]
structure = "bvh"
auto_rebuild = true
rebuild_threshold = 0.2        # Rebuild more frequently

[spatial.bvh]
max_leaf_size = 8              # Larger leaves = faster rebuild
build_quality = "fast"

[spatial.queries]
frustum_culling = true
occlusion_culling = false
max_query_results = 300
```

### Dense Uniform Scene

```toml
[spatial]
structure = "grid"             # Better for uniform distribution
auto_rebuild = true
rebuild_threshold = 0.4

[spatial.queries]
frustum_culling = true
occlusion_culling = false
max_query_results = 500
```

### Large Open World

```toml
[spatial]
structure = "octree"           # Good for large spaces
auto_rebuild = true
rebuild_threshold = 0.25

[spatial.queries]
frustum_culling = true
occlusion_culling = true
max_query_results = 1000
```

## Complete Spatial Configuration

```toml
# Scene file (scene.toml)
[spatial]
structure = "bvh"
auto_rebuild = true
rebuild_threshold = 0.3

[spatial.bvh]
max_leaf_size = 4
build_quality = "medium"

[spatial.queries]
frustum_culling = true
occlusion_culling = false
max_query_results = 500
```

```toml
# Manifest file (manifest.toml)
[app.spatial]
structure = "bvh"
rebuild_threshold = 0.3
max_depth = 32
min_objects_per_node = 2
```

## Frustum Culling

Objects outside the camera view are not rendered.

```toml
[spatial.queries]
frustum_culling = true         # Recommended: always enable
```

**Benefits:**
- Major performance improvement
- No visual difference
- Works automatically

## Occlusion Culling

Objects hidden behind other objects are not rendered.

```toml
[spatial.queries]
occlusion_culling = true       # Enable for complex scenes
```

**Trade-offs:**
- Significant performance gain in complex scenes
- Additional CPU/GPU overhead for occlusion tests
- Best for indoor/urban scenes with lots of occlusion

**When to Use:**
| Scene Type | Occlusion Culling |
|------------|-------------------|
| Open terrain | Off |
| Indoor | On |
| Urban | On |
| Space/Sky | Off |

## Query Result Limits

Prevent excessive results from spatial queries.

```toml
[spatial.queries]
max_query_results = 500        # Safety limit
```

**Guidelines:**
| Scene Complexity | Limit |
|------------------|-------|
| Simple (<100 entities) | 100 |
| Medium (100-1000 entities) | 500 |
| Complex (1000+ entities) | 1000 |

## Rebuild Threshold

Controls automatic spatial structure rebuilding.

```toml
rebuild_threshold = 0.3        # Rebuild when 30% of objects change
```

| Value | Behavior |
|-------|----------|
| `0.1` | Frequent rebuilds, better query performance |
| `0.3` | Balanced (default) |
| `0.5` | Less frequent rebuilds, lower overhead |
| `1.0` | Only rebuild when all objects change |
