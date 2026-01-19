//! Light Extraction System
//!
//! Extracts light data from ECS components and populates GPU buffers.
//! This module provides helper functions for light extraction without
//! directly depending on void_ecs.
//!
//! # Usage
//!
//! ```ignore
//! let mut buffer = LightBuffer::new();
//! buffer.begin_frame();
//!
//! // Extract lights (integration code)
//! for (entity, light, transform) in world.query::<(&Light, &Transform)>() {
//!     LightExtractor::extract_light(
//!         &light,
//!         transform.position,
//!         transform.forward(),
//!         &mut buffer,
//!         camera_position,
//!     );
//! }
//!
//! // Sort by importance
//! buffer.sort_point_lights_by_distance(camera_position);
//! ```

use crate::light_buffer::{
    LightBuffer, GpuDirectionalLight, GpuPointLight, GpuSpotLight,
    distance,
};

/// Light extraction configuration
#[derive(Clone, Debug)]
pub struct LightExtractionConfig {
    /// Enable distance culling for point/spot lights
    pub distance_culling: bool,
    /// Distance multiplier for culling (light.range * multiplier)
    pub culling_distance_multiplier: f32,
    /// Enable light sorting by importance
    pub sort_by_importance: bool,
    /// Maximum lights to process per frame
    pub max_lights_per_frame: usize,
}

impl Default for LightExtractionConfig {
    fn default() -> Self {
        Self {
            distance_culling: true,
            culling_distance_multiplier: 2.0,
            sort_by_importance: true,
            max_lights_per_frame: 512,
        }
    }
}

/// Light extractor helper
pub struct LightExtractor {
    config: LightExtractionConfig,
    lights_processed: usize,
    lights_culled: usize,
}

impl LightExtractor {
    /// Create a new light extractor with default config
    pub fn new() -> Self {
        Self::with_config(LightExtractionConfig::default())
    }

    /// Create with custom configuration
    pub fn with_config(config: LightExtractionConfig) -> Self {
        Self {
            config,
            lights_processed: 0,
            lights_culled: 0,
        }
    }

    /// Begin extraction for a new frame
    pub fn begin_frame(&mut self, buffer: &mut LightBuffer) {
        buffer.begin_frame();
        self.lights_processed = 0;
        self.lights_culled = 0;
    }

    /// Extract a directional light
    pub fn extract_directional(
        &mut self,
        buffer: &mut LightBuffer,
        direction: [f32; 3],
        color: [f32; 3],
        intensity: f32,
    ) -> bool {
        if self.lights_processed >= self.config.max_lights_per_frame {
            return false;
        }

        let light = GpuDirectionalLight::new(
            normalize(direction),
            color,
            intensity,
        );

        self.lights_processed += 1;
        buffer.add_directional(light)
    }

    /// Extract a point light with distance culling
    pub fn extract_point(
        &mut self,
        buffer: &mut LightBuffer,
        position: [f32; 3],
        range: f32,
        color: [f32; 3],
        intensity: f32,
        attenuation: [f32; 3],
        camera_pos: [f32; 3],
    ) -> bool {
        if self.lights_processed >= self.config.max_lights_per_frame {
            return false;
        }

        // Distance culling
        if self.config.distance_culling {
            let dist = distance(position, camera_pos);
            let cull_dist = range * self.config.culling_distance_multiplier;
            if dist > cull_dist {
                self.lights_culled += 1;
                return false;
            }
        }

        let light = GpuPointLight::new(
            position,
            range,
            color,
            intensity,
            attenuation,
        );

        self.lights_processed += 1;
        buffer.add_point(light)
    }

    /// Extract a spot light with distance culling
    pub fn extract_spot(
        &mut self,
        buffer: &mut LightBuffer,
        position: [f32; 3],
        direction: [f32; 3],
        range: f32,
        inner_angle: f32,
        outer_angle: f32,
        color: [f32; 3],
        intensity: f32,
        attenuation: [f32; 3],
        camera_pos: [f32; 3],
    ) -> bool {
        if self.lights_processed >= self.config.max_lights_per_frame {
            return false;
        }

        // Distance culling
        if self.config.distance_culling {
            let dist = distance(position, camera_pos);
            let cull_dist = range * self.config.culling_distance_multiplier;
            if dist > cull_dist {
                self.lights_culled += 1;
                return false;
            }
        }

        let light = GpuSpotLight::new(
            position,
            normalize(direction),
            range,
            inner_angle,
            outer_angle,
            color,
            intensity,
            attenuation,
        );

        self.lights_processed += 1;
        buffer.add_spot(light)
    }

    /// Finalize extraction (sort lights if enabled)
    pub fn finalize(&mut self, buffer: &mut LightBuffer, camera_pos: [f32; 3]) {
        if self.config.sort_by_importance {
            buffer.sort_point_lights_by_distance(camera_pos);
            buffer.sort_spot_lights_by_distance(camera_pos);
        }
    }

    /// Get number of lights processed
    pub fn lights_processed(&self) -> usize {
        self.lights_processed
    }

    /// Get number of lights culled
    pub fn lights_culled(&self) -> usize {
        self.lights_culled
    }

    /// Get extraction statistics
    pub fn stats(&self) -> LightExtractionStats {
        LightExtractionStats {
            lights_processed: self.lights_processed as u32,
            lights_culled: self.lights_culled as u32,
        }
    }
}

impl Default for LightExtractor {
    fn default() -> Self {
        Self::new()
    }
}

/// Light extraction statistics
#[derive(Clone, Debug, Default)]
pub struct LightExtractionStats {
    /// Total lights processed
    pub lights_processed: u32,
    /// Lights culled by distance
    pub lights_culled: u32,
}

/// Normalize a vector
fn normalize(v: [f32; 3]) -> [f32; 3] {
    let len = (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]).sqrt();
    if len > 0.0001 {
        [v[0] / len, v[1] / len, v[2] / len]
    } else {
        [0.0, -1.0, 0.0] // Default down direction
    }
}

/// Extracted light data for debugging/visualization
#[derive(Clone, Debug)]
pub struct ExtractedLightInfo {
    /// Light type (0=directional, 1=point, 2=spot)
    pub light_type: u32,
    /// World position
    pub position: [f32; 3],
    /// Direction (for directional/spot)
    pub direction: [f32; 3],
    /// Color
    pub color: [f32; 3],
    /// Intensity
    pub intensity: f32,
    /// Range (0 for directional)
    pub range: f32,
    /// Spot angles (inner, outer) in radians
    pub spot_angles: [f32; 2],
    /// Is shadow caster
    pub cast_shadows: bool,
}

impl From<&GpuDirectionalLight> for ExtractedLightInfo {
    fn from(light: &GpuDirectionalLight) -> Self {
        Self {
            light_type: 0,
            position: [0.0; 3],
            direction: light.direction,
            color: light.color,
            intensity: light.intensity,
            range: 0.0,
            spot_angles: [0.0; 2],
            cast_shadows: light.shadow_map_index >= 0,
        }
    }
}

impl From<&GpuPointLight> for ExtractedLightInfo {
    fn from(light: &GpuPointLight) -> Self {
        Self {
            light_type: 1,
            position: light.position,
            direction: [0.0; 3],
            color: light.color,
            intensity: light.intensity,
            range: light.range,
            spot_angles: [0.0; 2],
            cast_shadows: light.shadow_map_index >= 0,
        }
    }
}

impl From<&GpuSpotLight> for ExtractedLightInfo {
    fn from(light: &GpuSpotLight) -> Self {
        Self {
            light_type: 2,
            position: light.position,
            direction: light.direction,
            color: light.color,
            intensity: light.intensity,
            range: light.range,
            spot_angles: [light.inner_cos.acos(), light.outer_cos.acos()],
            cast_shadows: light.shadow_map_index >= 0,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_light_extractor_basic() {
        let mut extractor = LightExtractor::new();
        let mut buffer = LightBuffer::new();
        extractor.begin_frame(&mut buffer);

        // Add directional
        assert!(extractor.extract_directional(
            &mut buffer,
            [0.0, -1.0, 0.0],
            [1.0, 1.0, 1.0],
            1.0,
        ));

        // Add point
        assert!(extractor.extract_point(
            &mut buffer,
            [0.0, 5.0, 0.0],
            10.0,
            [1.0, 0.8, 0.6],
            1000.0,
            [1.0, 0.0, 1.0],
            [0.0, 0.0, 0.0],
        ));

        extractor.finalize(&mut buffer, [0.0, 0.0, 0.0]);

        assert_eq!(buffer.directional_lights.len(), 1);
        assert_eq!(buffer.point_lights.len(), 1);
        assert_eq!(extractor.lights_processed(), 2);
    }

    #[test]
    fn test_distance_culling() {
        let mut extractor = LightExtractor::new();
        let mut buffer = LightBuffer::new();
        extractor.begin_frame(&mut buffer);

        // Light at origin with range 10
        // Camera far away - should be culled
        let added = extractor.extract_point(
            &mut buffer,
            [0.0, 0.0, 0.0],
            10.0,
            [1.0; 3],
            1000.0,
            [1.0, 0.0, 1.0],
            [100.0, 0.0, 0.0], // Camera at x=100
        );

        assert!(!added);
        assert_eq!(extractor.lights_culled(), 1);

        // Camera close - should not be culled
        let added = extractor.extract_point(
            &mut buffer,
            [0.0, 0.0, 0.0],
            10.0,
            [1.0; 3],
            1000.0,
            [1.0, 0.0, 1.0],
            [5.0, 0.0, 0.0], // Camera at x=5
        );

        assert!(added);
    }

    #[test]
    fn test_spot_light_extraction() {
        let mut extractor = LightExtractor::new();
        let mut buffer = LightBuffer::new();
        extractor.begin_frame(&mut buffer);

        assert!(extractor.extract_spot(
            &mut buffer,
            [0.0, 10.0, 0.0],
            [0.0, -1.0, 0.0],
            20.0,
            30.0_f32.to_radians(),
            45.0_f32.to_radians(),
            [1.0, 1.0, 0.9],
            2000.0,
            [1.0, 0.0, 1.0],
            [0.0, 0.0, 0.0],
        ));

        assert_eq!(buffer.spot_lights.len(), 1);

        let spot = &buffer.spot_lights[0];
        assert!((spot.inner_cos - 30.0_f32.to_radians().cos()).abs() < 0.01);
    }

    #[test]
    fn test_normalize() {
        let n = normalize([3.0, 4.0, 0.0]);
        let len = (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]).sqrt();
        assert!((len - 1.0).abs() < 0.001);
    }

    #[test]
    fn test_max_lights_limit() {
        let config = LightExtractionConfig {
            max_lights_per_frame: 3,
            distance_culling: false,
            ..Default::default()
        };
        let mut extractor = LightExtractor::with_config(config);
        let mut buffer = LightBuffer::new();
        extractor.begin_frame(&mut buffer);

        // Add 3 lights (at limit)
        for _ in 0..3 {
            assert!(extractor.extract_point(
                &mut buffer,
                [0.0; 3],
                10.0,
                [1.0; 3],
                100.0,
                [1.0, 0.0, 1.0],
                [0.0; 3],
            ));
        }

        // 4th should fail due to limit
        assert!(!extractor.extract_point(
            &mut buffer,
            [0.0; 3],
            10.0,
            [1.0; 3],
            100.0,
            [1.0, 0.0, 1.0],
            [0.0; 3],
        ));

        assert_eq!(extractor.lights_processed(), 3);
    }
}
