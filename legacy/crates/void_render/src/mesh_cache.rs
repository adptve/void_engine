//! GPU Mesh Cache
//!
//! Manages GPU buffer resources for mesh data:
//! - Automatic buffer creation and upload
//! - Reference counting for shared meshes
//! - LRU eviction for memory management
//! - Hot-reload support

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};

/// GPU buffer handle for vertex data
#[derive(Clone, Debug)]
pub struct GpuVertexBuffer {
    /// Unique buffer ID
    pub id: u64,
    /// Vertex count
    pub vertex_count: u32,
    /// Stride in bytes
    pub stride: u32,
    /// Buffer size in bytes
    pub size: u64,
}

/// GPU buffer handle for index data
#[derive(Clone, Debug)]
pub struct GpuIndexBuffer {
    /// Unique buffer ID
    pub id: u64,
    /// Index count
    pub index_count: u32,
    /// Index format (U16 or U32)
    pub format: IndexFormat,
    /// Buffer size in bytes
    pub size: u64,
}

/// Index buffer format
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum IndexFormat {
    /// 16-bit unsigned indices
    U16,
    /// 32-bit unsigned indices
    U32,
}

/// A cached mesh on the GPU
#[derive(Clone, Debug)]
pub struct CachedMesh {
    /// Mesh asset ID
    pub asset_id: u64,
    /// Asset path for hot-reload
    pub path: String,
    /// GPU primitives
    pub primitives: Vec<CachedPrimitive>,
    /// Total GPU memory used
    pub gpu_memory: u64,
    /// Reference count
    pub ref_count: u32,
    /// Last access frame
    pub last_access_frame: u64,
}

/// A cached primitive on the GPU
#[derive(Clone, Debug)]
pub struct CachedPrimitive {
    /// Primitive index
    pub index: u32,
    /// Vertex buffer
    pub vertex_buffer: GpuVertexBuffer,
    /// Index buffer (None if non-indexed)
    pub index_buffer: Option<GpuIndexBuffer>,
    /// Triangle count
    pub triangle_count: u32,
    /// Material index
    pub material_index: Option<u32>,
}

/// Handle to a cached mesh
#[derive(Clone, Debug)]
pub struct MeshHandle {
    /// Asset ID
    pub asset_id: u64,
    /// Generation for validity checking
    pub generation: u64,
}

impl MeshHandle {
    /// Check if this handle is valid
    pub fn is_valid(&self) -> bool {
        self.generation > 0
    }
}

/// GPU mesh cache
pub struct MeshCache {
    /// Cached meshes by asset ID
    meshes: HashMap<u64, CachedMesh>,
    /// Path to asset ID mapping
    path_to_id: HashMap<String, u64>,
    /// Next buffer ID
    next_buffer_id: AtomicU64,
    /// Next asset ID
    next_asset_id: AtomicU64,
    /// Current frame number
    current_frame: u64,
    /// Maximum GPU memory budget (bytes)
    memory_budget: u64,
    /// Current GPU memory usage (bytes)
    memory_usage: u64,
    /// Generation counter for handles
    generation: u64,
}

impl MeshCache {
    /// Create a new mesh cache with the given memory budget
    pub fn new(memory_budget_mb: u64) -> Self {
        Self {
            meshes: HashMap::new(),
            path_to_id: HashMap::new(),
            next_buffer_id: AtomicU64::new(1),
            next_asset_id: AtomicU64::new(1),
            current_frame: 0,
            memory_budget: memory_budget_mb * 1024 * 1024,
            memory_usage: 0,
            generation: 1,
        }
    }

    /// Allocate a new buffer ID
    fn alloc_buffer_id(&self) -> u64 {
        self.next_buffer_id.fetch_add(1, Ordering::Relaxed)
    }

    /// Allocate a new asset ID
    fn alloc_asset_id(&self) -> u64 {
        self.next_asset_id.fetch_add(1, Ordering::Relaxed)
    }

    /// Get or create a mesh handle by path
    pub fn get_or_load(&mut self, path: &str) -> Option<MeshHandle> {
        if let Some(&asset_id) = self.path_to_id.get(path) {
            if let Some(mesh) = self.meshes.get_mut(&asset_id) {
                mesh.ref_count += 1;
                mesh.last_access_frame = self.current_frame;
                return Some(MeshHandle {
                    asset_id,
                    generation: self.generation,
                });
            }
        }
        None
    }

    /// Insert a mesh into the cache
    ///
    /// Returns a handle to the cached mesh. The caller is responsible for
    /// uploading the actual GPU buffers - this just tracks the metadata.
    pub fn insert(
        &mut self,
        path: &str,
        primitives: Vec<CachedPrimitive>,
        gpu_memory: u64,
    ) -> MeshHandle {
        let asset_id = self.alloc_asset_id();

        // Evict if needed
        while self.memory_usage + gpu_memory > self.memory_budget {
            if !self.evict_lru() {
                log::warn!("Mesh cache: Cannot evict, over budget");
                break;
            }
        }

        let cached_mesh = CachedMesh {
            asset_id,
            path: path.to_string(),
            primitives,
            gpu_memory,
            ref_count: 1,
            last_access_frame: self.current_frame,
        };

        self.memory_usage += gpu_memory;
        self.path_to_id.insert(path.to_string(), asset_id);
        self.meshes.insert(asset_id, cached_mesh);

        MeshHandle {
            asset_id,
            generation: self.generation,
        }
    }

    /// Create GPU primitive metadata for a mesh primitive
    pub fn create_primitive(
        &self,
        index: u32,
        vertex_count: u32,
        vertex_stride: u32,
        index_count: Option<u32>,
        material_index: Option<u32>,
    ) -> CachedPrimitive {
        let vertex_size = (vertex_count as u64) * (vertex_stride as u64);

        let (index_buffer, index_size) = if let Some(count) = index_count {
            let format = if vertex_count > 65535 {
                IndexFormat::U32
            } else {
                IndexFormat::U16
            };
            let size = (count as u64) * if format == IndexFormat::U32 { 4 } else { 2 };

            (Some(GpuIndexBuffer {
                id: self.alloc_buffer_id(),
                index_count: count,
                format,
                size,
            }), size)
        } else {
            (None, 0)
        };

        let triangle_count = index_count.unwrap_or(vertex_count) / 3;

        CachedPrimitive {
            index,
            vertex_buffer: GpuVertexBuffer {
                id: self.alloc_buffer_id(),
                vertex_count,
                stride: vertex_stride,
                size: vertex_size,
            },
            index_buffer,
            triangle_count,
            material_index,
        }
    }

    /// Get a cached mesh by handle
    pub fn get(&mut self, handle: &MeshHandle) -> Option<&CachedMesh> {
        if handle.generation != self.generation {
            return None;
        }
        if let Some(mesh) = self.meshes.get_mut(&handle.asset_id) {
            mesh.last_access_frame = self.current_frame;
            Some(mesh)
        } else {
            None
        }
    }

    /// Get a cached mesh by asset ID
    pub fn get_by_id(&mut self, asset_id: u64) -> Option<&CachedMesh> {
        if let Some(mesh) = self.meshes.get_mut(&asset_id) {
            mesh.last_access_frame = self.current_frame;
            Some(mesh)
        } else {
            None
        }
    }

    /// Release a mesh handle (decrement ref count)
    pub fn release(&mut self, handle: MeshHandle) {
        if let Some(mesh) = self.meshes.get_mut(&handle.asset_id) {
            mesh.ref_count = mesh.ref_count.saturating_sub(1);
        }
    }

    /// Advance to next frame
    pub fn end_frame(&mut self) {
        self.current_frame += 1;
    }

    /// Evict the least recently used mesh with zero references
    fn evict_lru(&mut self) -> bool {
        // Find LRU mesh with zero references
        let mut lru_id = None;
        let mut lru_frame = u64::MAX;

        for (id, mesh) in &self.meshes {
            if mesh.ref_count == 0 && mesh.last_access_frame < lru_frame {
                lru_frame = mesh.last_access_frame;
                lru_id = Some(*id);
            }
        }

        if let Some(id) = lru_id {
            self.remove(id);
            true
        } else {
            false
        }
    }

    /// Remove a mesh from the cache
    pub fn remove(&mut self, asset_id: u64) {
        if let Some(mesh) = self.meshes.remove(&asset_id) {
            self.path_to_id.remove(&mesh.path);
            self.memory_usage = self.memory_usage.saturating_sub(mesh.gpu_memory);
        }
    }

    /// Hot-reload a mesh (invalidates existing handles)
    pub fn reload(&mut self, path: &str) {
        if let Some(&asset_id) = self.path_to_id.get(path) {
            self.remove(asset_id);
        }
        // Increment generation to invalidate old handles
        self.generation += 1;
    }

    /// Clear all cached meshes
    pub fn clear(&mut self) {
        self.meshes.clear();
        self.path_to_id.clear();
        self.memory_usage = 0;
        self.generation += 1;
    }

    /// Get current memory usage in bytes
    pub fn memory_usage(&self) -> u64 {
        self.memory_usage
    }

    /// Get memory budget in bytes
    pub fn memory_budget(&self) -> u64 {
        self.memory_budget
    }

    /// Get number of cached meshes
    pub fn mesh_count(&self) -> usize {
        self.meshes.len()
    }

    /// Get cache statistics
    pub fn stats(&self) -> MeshCacheStats {
        let mut total_vertices = 0;
        let mut total_triangles = 0;
        let mut total_primitives = 0;

        for mesh in self.meshes.values() {
            for prim in &mesh.primitives {
                total_vertices += prim.vertex_buffer.vertex_count as u64;
                total_triangles += prim.triangle_count as u64;
                total_primitives += 1;
            }
        }

        MeshCacheStats {
            mesh_count: self.meshes.len(),
            primitive_count: total_primitives,
            vertex_count: total_vertices,
            triangle_count: total_triangles,
            memory_usage: self.memory_usage,
            memory_budget: self.memory_budget,
            current_frame: self.current_frame,
        }
    }
}

impl Default for MeshCache {
    fn default() -> Self {
        Self::new(256) // 256 MB default budget
    }
}

/// Mesh cache statistics
#[derive(Clone, Debug, Default)]
pub struct MeshCacheStats {
    /// Number of cached meshes
    pub mesh_count: usize,
    /// Total primitive count
    pub primitive_count: usize,
    /// Total vertex count
    pub vertex_count: u64,
    /// Total triangle count
    pub triangle_count: u64,
    /// Current memory usage (bytes)
    pub memory_usage: u64,
    /// Memory budget (bytes)
    pub memory_budget: u64,
    /// Current frame number
    pub current_frame: u64,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_mesh_cache_basic() {
        let mut cache = MeshCache::new(64); // 64 MB

        // Create a primitive
        let prim = cache.create_primitive(0, 100, 48, Some(300), Some(0));

        // Insert mesh
        let handle = cache.insert("test/cube.obj", vec![prim.clone()], 5000);

        assert!(handle.is_valid());
        assert_eq!(cache.mesh_count(), 1);

        // Get mesh
        let mesh = cache.get(&handle).expect("Mesh should exist");
        assert_eq!(mesh.primitives.len(), 1);
        assert_eq!(mesh.primitives[0].vertex_buffer.vertex_count, 100);
    }

    #[test]
    fn test_mesh_cache_lru_eviction() {
        let mut cache = MeshCache::new(1); // 1 MB - small budget

        // Insert multiple meshes
        for i in 0..10 {
            let prim = cache.create_primitive(0, 1000, 48, Some(3000), None);
            let path = format!("test/mesh_{}.obj", i);
            let handle = cache.insert(&path, vec![prim], 200_000); // 200KB each

            // Release handle to allow eviction
            cache.release(handle);
            cache.end_frame();
        }

        // Some meshes should have been evicted
        assert!(cache.memory_usage() <= cache.memory_budget());
    }

    #[test]
    fn test_mesh_cache_reload() {
        let mut cache = MeshCache::new(64);

        let prim = cache.create_primitive(0, 100, 48, Some(300), Some(0));
        let handle1 = cache.insert("test/cube.obj", vec![prim.clone()], 5000);

        let gen1 = handle1.generation;

        // Reload the mesh
        cache.reload("test/cube.obj");

        // Old handle should be invalid (different generation)
        let handle2 = cache.insert("test/cube.obj", vec![prim], 5000);

        assert_ne!(handle1.generation, handle2.generation);
        assert!(cache.get(&handle1).is_none());
    }
}
