//! Render Extraction System
//!
//! Extracts renderable entities from the ECS World and builds draw lists
//! for GPU rendering. This is the bridge between game logic (ECS) and
//! rendering (GPU).
//!
//! # Architecture
//!
//! ```text
//! ECS World ──► Extraction ──► Draw Lists ──► GPU Commands
//!     │              │              │
//!     │        Query for:       Sort by:
//!     │        - Transform      - Layer
//!     │        - MeshRenderer   - Material
//!     │        - Material       - Depth
//!     │        - Visible
//! ```
//!
//! # Usage
//!
//! ```ignore
//! use void_render::extraction::{RenderExtractor, ExtractedScene};
//! use void_ecs::World;
//!
//! let mut world = World::new();
//! // ... spawn renderable entities ...
//!
//! let extractor = RenderExtractor::new(&mut world);
//! let scene = extractor.extract(&world, camera_position);
//!
//! // Use extracted data for rendering
//! for draw in scene.draw_calls {
//!     // Issue GPU draw command
//! }
//! ```

use alloc::string::String;
use alloc::vec::Vec;

/// An extracted draw call ready for GPU submission
#[derive(Clone, Debug)]
pub struct DrawCall {
    /// Entity ID (for debugging)
    pub entity_id: u64,
    /// Model matrix (4x4 column-major)
    pub model_matrix: [[f32; 4]; 4],
    /// Mesh type identifier
    pub mesh_type: MeshTypeId,
    /// Material properties
    pub material: ExtractedMaterial,
    /// Distance from camera (for sorting)
    pub camera_distance: f32,
    /// Layer assignment
    pub layer: String,
}

/// Mesh type identifier for draw calls
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum MeshTypeId {
    /// Built-in cube
    Cube,
    /// Built-in sphere
    Sphere,
    /// Built-in cylinder
    Cylinder,
    /// Built-in plane
    Plane,
    /// Built-in quad
    Quad,
    /// Custom mesh (requires lookup)
    Custom(u64),
}

/// Extracted material properties
#[derive(Clone, Debug)]
pub struct ExtractedMaterial {
    /// Base color (RGBA)
    pub base_color: [f32; 4],
    /// Emissive color (RGB)
    pub emissive: [f32; 3],
    /// Metallic factor
    pub metallic: f32,
    /// Roughness factor
    pub roughness: f32,
    /// Custom shader name
    pub shader: Option<String>,
}

impl Default for ExtractedMaterial {
    fn default() -> Self {
        Self {
            base_color: [1.0, 1.0, 1.0, 1.0],
            emissive: [0.0, 0.0, 0.0],
            metallic: 0.0,
            roughness: 0.5,
            shader: None,
        }
    }
}

/// An extracted light source
#[derive(Clone, Copy, Debug)]
pub struct ExtractedLight {
    /// World position
    pub position: [f32; 3],
    /// Direction (for directional/spot)
    pub direction: [f32; 3],
    /// Color (RGB)
    pub color: [f32; 3],
    /// Intensity
    pub intensity: f32,
    /// Light type
    pub light_type: LightTypeId,
    /// Range (for point/spot)
    pub range: f32,
    /// Angles (inner, outer) for spot lights
    pub spot_angles: [f32; 2],
    /// Cast shadows
    pub cast_shadows: bool,
}

/// Light type identifier
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LightTypeId {
    Directional,
    Point,
    Spot,
}

/// The complete extracted scene for rendering
#[derive(Clone, Debug)]
pub struct ExtractedScene {
    /// All draw calls, sorted by layer then material
    pub draw_calls: Vec<DrawCall>,
    /// All light sources
    pub lights: Vec<ExtractedLight>,
    /// Camera view matrix
    pub view_matrix: [[f32; 4]; 4],
    /// Camera projection matrix
    pub projection_matrix: [[f32; 4]; 4],
    /// Frame number
    pub frame: u64,
    /// Total entity count processed
    pub entity_count: usize,
    /// Culled entity count
    pub culled_count: usize,
}

impl ExtractedScene {
    /// Create an empty scene
    pub fn empty(frame: u64) -> Self {
        Self {
            draw_calls: Vec::new(),
            lights: Vec::new(),
            view_matrix: IDENTITY_MATRIX,
            projection_matrix: IDENTITY_MATRIX,
            frame,
            entity_count: 0,
            culled_count: 0,
        }
    }

    /// Get draw calls grouped by layer
    pub fn draws_by_layer(&self) -> alloc::collections::BTreeMap<String, Vec<&DrawCall>> {
        let mut map = alloc::collections::BTreeMap::new();
        for draw in &self.draw_calls {
            map.entry(draw.layer.clone())
                .or_insert_with(Vec::new)
                .push(draw);
        }
        map
    }

    /// Get opaque draw calls (alpha = 1.0)
    pub fn opaque_draws(&self) -> Vec<&DrawCall> {
        self.draw_calls
            .iter()
            .filter(|d| d.material.base_color[3] >= 1.0)
            .collect()
    }

    /// Get transparent draw calls (alpha < 1.0), sorted back-to-front
    pub fn transparent_draws(&self) -> Vec<&DrawCall> {
        let mut draws: Vec<_> = self.draw_calls
            .iter()
            .filter(|d| d.material.base_color[3] < 1.0)
            .collect();
        // Sort back to front for proper blending
        draws.sort_by(|a, b| b.camera_distance.partial_cmp(&a.camera_distance).unwrap());
        draws
    }
}

/// Identity matrix constant
const IDENTITY_MATRIX: [[f32; 4]; 4] = [
    [1.0, 0.0, 0.0, 0.0],
    [0.0, 1.0, 0.0, 0.0],
    [0.0, 0.0, 1.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
];

/// Configuration for render extraction
#[derive(Clone, Debug)]
pub struct ExtractionConfig {
    /// Enable frustum culling
    pub frustum_culling: bool,
    /// Maximum draw calls per frame
    pub max_draw_calls: usize,
    /// Default layer name
    pub default_layer: String,
}

impl Default for ExtractionConfig {
    fn default() -> Self {
        Self {
            frustum_culling: true,
            max_draw_calls: 10000,
            default_layer: "world".to_string(),
        }
    }
}

/// Render extractor that queries ECS and builds draw lists
///
/// This is designed to work with void_ecs components but doesn't
/// directly depend on void_ecs types to avoid circular dependencies.
pub struct RenderExtractor {
    config: ExtractionConfig,
    frame: u64,
}

impl RenderExtractor {
    /// Create a new render extractor
    pub fn new(config: ExtractionConfig) -> Self {
        Self {
            config,
            frame: 0,
        }
    }

    /// Begin extraction for a new frame
    pub fn begin_frame(&mut self) {
        self.frame += 1;
    }

    /// Create a draw call from component data
    ///
    /// This is called by the integration code that has access to void_ecs.
    pub fn create_draw_call(
        &self,
        entity_id: u64,
        model_matrix: [[f32; 4]; 4],
        mesh_type: MeshTypeId,
        material: ExtractedMaterial,
        camera_position: [f32; 3],
        layer: Option<&str>,
    ) -> DrawCall {
        // Calculate camera distance from model position (last column of matrix)
        let pos = [model_matrix[3][0], model_matrix[3][1], model_matrix[3][2]];
        let dx = pos[0] - camera_position[0];
        let dy = pos[1] - camera_position[1];
        let dz = pos[2] - camera_position[2];
        let camera_distance = (dx * dx + dy * dy + dz * dz).sqrt();

        DrawCall {
            entity_id,
            model_matrix,
            mesh_type,
            material,
            camera_distance,
            layer: layer.unwrap_or(&self.config.default_layer).to_string(),
        }
    }

    /// Create a light from component data
    pub fn create_light(
        &self,
        position: [f32; 3],
        direction: [f32; 3],
        color: [f32; 3],
        intensity: f32,
        light_type: LightTypeId,
        range: f32,
        spot_angles: [f32; 2],
        cast_shadows: bool,
    ) -> ExtractedLight {
        ExtractedLight {
            position,
            direction,
            color,
            intensity,
            light_type,
            range,
            spot_angles,
            cast_shadows,
        }
    }

    /// Build the extracted scene from collected draw calls and lights
    pub fn build_scene(
        &self,
        mut draw_calls: Vec<DrawCall>,
        lights: Vec<ExtractedLight>,
        view_matrix: [[f32; 4]; 4],
        projection_matrix: [[f32; 4]; 4],
        total_entities: usize,
        culled_entities: usize,
    ) -> ExtractedScene {
        // Sort draw calls: by layer, then by shader, then front-to-back for opaques
        draw_calls.sort_by(|a, b| {
            // First by layer
            match a.layer.cmp(&b.layer) {
                core::cmp::Ordering::Equal => {}
                other => return other,
            }
            // Then by shader (for batching)
            match (&a.material.shader, &b.material.shader) {
                (Some(a_shader), Some(b_shader)) => match a_shader.cmp(b_shader) {
                    core::cmp::Ordering::Equal => {}
                    other => return other,
                },
                _ => {}
            }
            // Then front-to-back for opaques (reduces overdraw)
            if a.material.base_color[3] >= 1.0 && b.material.base_color[3] >= 1.0 {
                a.camera_distance.partial_cmp(&b.camera_distance).unwrap_or(core::cmp::Ordering::Equal)
            } else {
                core::cmp::Ordering::Equal
            }
        });

        // Limit draw calls
        if draw_calls.len() > self.config.max_draw_calls {
            draw_calls.truncate(self.config.max_draw_calls);
        }

        ExtractedScene {
            draw_calls,
            lights,
            view_matrix,
            projection_matrix,
            frame: self.frame,
            entity_count: total_entities,
            culled_count: culled_entities,
        }
    }

    /// Get current frame number
    pub fn frame(&self) -> u64 {
        self.frame
    }

    /// Get extraction configuration
    pub fn config(&self) -> &ExtractionConfig {
        &self.config
    }

    /// Add an entity to an instance batch
    ///
    /// This is called by integration code that has access to void_ecs.
    /// Returns true if the instance was added successfully.
    pub fn add_to_instance_batch(
        &self,
        batcher: &mut crate::instance_batcher::InstanceBatcher,
        entity_id: u64,
        mesh_id: u64,
        material_id: u64,
        layer_mask: u32,
        model_matrix: [[f32; 4]; 4],
        color_tint: [f32; 4],
        custom_data: Option<[f32; 4]>,
    ) -> bool {
        if let Some(custom) = custom_data {
            batcher.add_instance_full(
                entity_id,
                mesh_id,
                material_id,
                layer_mask,
                model_matrix,
                color_tint,
                custom,
            )
        } else {
            batcher.add_instance_with_layer(
                entity_id,
                mesh_id,
                material_id,
                layer_mask,
                model_matrix,
                color_tint,
            )
        }
    }

    /// Build draw commands from instance batches
    pub fn build_instanced_draw_commands(
        &self,
        batcher: &crate::instance_batcher::InstanceBatcher,
        mesh_info: &dyn Fn(u64) -> Option<MeshInfo>,
    ) -> crate::draw_command::DrawList {
        use crate::draw_command::{DrawCommand, DrawList};
        use crate::instancing::BatchKey;

        let mut draw_list = DrawList::new();

        for (key, batch) in batcher.batches() {
            if batch.is_empty() {
                continue;
            }

            // Get mesh info to determine index/vertex counts
            if let Some(info) = mesh_info(key.mesh_id) {
                let cmd = if info.indexed {
                    DrawCommand::instanced_indexed(
                        *key,
                        0, // primitive index
                        info.index_count,
                        batch.len() as u32,
                    )
                } else {
                    DrawCommand::instanced_non_indexed(
                        *key,
                        0,
                        info.vertex_count,
                        batch.len() as u32,
                    )
                };

                // TODO: Separate by transparency
                draw_list.add_opaque(cmd);
            }
        }

        draw_list
    }
}

/// Information about a mesh for draw command generation
#[derive(Clone, Copy, Debug)]
pub struct MeshInfo {
    /// Whether the mesh is indexed
    pub indexed: bool,
    /// Index count (if indexed)
    pub index_count: u32,
    /// Vertex count
    pub vertex_count: u32,
    /// Primitive count
    pub primitive_count: u32,
}

impl Default for MeshInfo {
    fn default() -> Self {
        Self {
            indexed: true,
            index_count: 36, // Default cube
            vertex_count: 24,
            primitive_count: 1,
        }
    }
}

impl Default for RenderExtractor {
    fn default() -> Self {
        Self::new(ExtractionConfig::default())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_extracted_scene_empty() {
        let scene = ExtractedScene::empty(1);
        assert!(scene.draw_calls.is_empty());
        assert!(scene.lights.is_empty());
        assert_eq!(scene.frame, 1);
    }

    #[test]
    fn test_draw_call_sorting() {
        let extractor = RenderExtractor::default();
        let camera_pos = [0.0, 0.0, 0.0];

        let mut draws = vec![
            extractor.create_draw_call(
                1,
                [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [10.0, 0.0, 0.0, 1.0]],
                MeshTypeId::Cube,
                ExtractedMaterial::default(),
                camera_pos,
                Some("world"),
            ),
            extractor.create_draw_call(
                2,
                [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [5.0, 0.0, 0.0, 1.0]],
                MeshTypeId::Sphere,
                ExtractedMaterial::default(),
                camera_pos,
                Some("world"),
            ),
        ];

        let scene = extractor.build_scene(
            draws,
            vec![],
            IDENTITY_MATRIX,
            IDENTITY_MATRIX,
            2,
            0,
        );

        // Closer entity should come first (front-to-back sorting)
        assert_eq!(scene.draw_calls[0].entity_id, 2);
        assert_eq!(scene.draw_calls[1].entity_id, 1);
    }

    #[test]
    fn test_transparent_sorting() {
        let extractor = RenderExtractor::default();
        let camera_pos = [0.0, 0.0, 0.0];

        let mut mat_transparent = ExtractedMaterial::default();
        mat_transparent.base_color[3] = 0.5;

        let draws = vec![
            extractor.create_draw_call(
                1,
                [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [5.0, 0.0, 0.0, 1.0]],
                MeshTypeId::Cube,
                mat_transparent.clone(),
                camera_pos,
                None,
            ),
            extractor.create_draw_call(
                2,
                [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [10.0, 0.0, 0.0, 1.0]],
                MeshTypeId::Cube,
                mat_transparent,
                camera_pos,
                None,
            ),
        ];

        let scene = extractor.build_scene(
            draws,
            vec![],
            IDENTITY_MATRIX,
            IDENTITY_MATRIX,
            2,
            0,
        );

        // Transparent draws should be sorted back-to-front
        let transparent = scene.transparent_draws();
        assert_eq!(transparent[0].entity_id, 2); // Farther first
        assert_eq!(transparent[1].entity_id, 1); // Closer second
    }

    #[test]
    fn test_instanced_extraction() {
        use crate::instance_batcher::InstanceBatcher;

        let extractor = RenderExtractor::default();
        let mut batcher = InstanceBatcher::new(1000);
        batcher.begin_frame();

        let matrix = IDENTITY_MATRIX;

        // Add 10 instances of same mesh
        for i in 0..10 {
            let added = extractor.add_to_instance_batch(
                &mut batcher,
                i,
                1, // mesh_id
                0, // material_id
                1, // layer_mask
                matrix,
                [1.0, 1.0, 1.0, 1.0],
                None,
            );
            assert!(added);
        }

        assert_eq!(batcher.total_instances(), 10);

        // Build draw commands
        let draw_list = extractor.build_instanced_draw_commands(
            &batcher,
            &|mesh_id| {
                if mesh_id == 1 {
                    Some(MeshInfo::default())
                } else {
                    None
                }
            },
        );

        assert_eq!(draw_list.opaque_instanced.len(), 1);
        assert_eq!(draw_list.opaque_instanced[0].instance_count(), 10);
    }
}
