//! Texture Manager - Loads and caches GPU textures
//!
//! Supports loading PNG, JPG, and other image formats
//! Creates GPU textures with mipmaps and proper sampling

use std::collections::HashMap;
use std::path::{Path, PathBuf};
use wgpu::*;
use image::{GenericImageView, DynamicImage};

/// Handle to a loaded texture
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, Default)]
pub struct TextureHandle(pub u32);

impl TextureHandle {
    pub const INVALID: Self = Self(u32::MAX);

    pub fn is_valid(&self) -> bool {
        self.0 != u32::MAX
    }
}

// Default returns invalid handle
impl From<u32> for TextureHandle {
    fn from(id: u32) -> Self {
        Self(id)
    }
}

/// A loaded GPU texture with its view and sampler
pub struct GpuTexture {
    pub texture: Texture,
    pub view: TextureView,
    pub sampler: Sampler,
    pub size: (u32, u32),
    pub format: TextureFormat,
}

/// Texture loading options
#[derive(Clone, Debug)]
pub struct TextureOptions {
    /// Generate mipmaps
    pub generate_mipmaps: bool,
    /// Texture format (None = auto-detect from image)
    pub format: Option<TextureFormat>,
    /// Address mode for UV coordinates
    pub address_mode: AddressMode,
    /// Filter mode for sampling
    pub filter_mode: FilterMode,
    /// Is this a normal map (affects format choice)
    pub is_normal_map: bool,
    /// Is this a linear (non-sRGB) texture
    pub is_linear: bool,
}

impl Default for TextureOptions {
    fn default() -> Self {
        Self {
            generate_mipmaps: true,
            format: None,
            address_mode: AddressMode::Repeat,
            filter_mode: FilterMode::Linear,
            is_normal_map: false,
            is_linear: false,
        }
    }
}

impl TextureOptions {
    pub fn normal_map() -> Self {
        Self {
            is_normal_map: true,
            is_linear: true,
            ..Default::default()
        }
    }

    pub fn linear() -> Self {
        Self {
            is_linear: true,
            ..Default::default()
        }
    }
}

/// Manages texture loading and caching
pub struct TextureManager {
    textures: HashMap<TextureHandle, GpuTexture>,
    path_to_handle: HashMap<PathBuf, TextureHandle>,
    next_handle: u32,

    /// Default 1x1 white texture
    pub default_white: TextureHandle,
    /// Default 1x1 normal map (flat normal pointing up)
    pub default_normal: TextureHandle,
    /// Default 1x1 black texture
    pub default_black: TextureHandle,
}

impl TextureManager {
    /// Create a new texture manager with default textures
    pub fn new(device: &Device, queue: &Queue) -> Self {
        let mut manager = Self {
            textures: HashMap::new(),
            path_to_handle: HashMap::new(),
            next_handle: 0,
            default_white: TextureHandle::INVALID,
            default_normal: TextureHandle::INVALID,
            default_black: TextureHandle::INVALID,
        };

        // Create default textures
        manager.default_white = manager.create_solid_color(device, queue, [255, 255, 255, 255], false);
        manager.default_normal = manager.create_solid_color(device, queue, [128, 128, 255, 255], true);
        manager.default_black = manager.create_solid_color(device, queue, [0, 0, 0, 255], false);

        log::info!("TextureManager initialized with default textures");
        manager
    }

    /// Create a 1x1 solid color texture
    fn create_solid_color(&mut self, device: &Device, queue: &Queue, color: [u8; 4], is_linear: bool) -> TextureHandle {
        let format = if is_linear {
            TextureFormat::Rgba8Unorm
        } else {
            TextureFormat::Rgba8UnormSrgb
        };

        let texture = device.create_texture(&TextureDescriptor {
            label: Some("solid_color_texture"),
            size: Extent3d {
                width: 1,
                height: 1,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format,
            usage: TextureUsages::TEXTURE_BINDING | TextureUsages::COPY_DST,
            view_formats: &[],
        });

        queue.write_texture(
            ImageCopyTexture {
                texture: &texture,
                mip_level: 0,
                origin: Origin3d::ZERO,
                aspect: TextureAspect::All,
            },
            &color,
            ImageDataLayout {
                offset: 0,
                bytes_per_row: Some(4),
                rows_per_image: Some(1),
            },
            Extent3d {
                width: 1,
                height: 1,
                depth_or_array_layers: 1,
            },
        );

        let view = texture.create_view(&TextureViewDescriptor::default());
        let sampler = device.create_sampler(&SamplerDescriptor {
            address_mode_u: AddressMode::Repeat,
            address_mode_v: AddressMode::Repeat,
            address_mode_w: AddressMode::Repeat,
            mag_filter: FilterMode::Nearest,
            min_filter: FilterMode::Nearest,
            mipmap_filter: FilterMode::Nearest,
            ..Default::default()
        });

        let handle = TextureHandle(self.next_handle);
        self.next_handle += 1;

        self.textures.insert(handle, GpuTexture {
            texture,
            view,
            sampler,
            size: (1, 1),
            format,
        });

        handle
    }

    /// Load a texture from a file path
    pub fn load_from_file(
        &mut self,
        device: &Device,
        queue: &Queue,
        path: &Path,
        options: &TextureOptions,
    ) -> Result<TextureHandle, String> {
        // Check cache first
        if let Some(&handle) = self.path_to_handle.get(path) {
            return Ok(handle);
        }

        // Load image
        let img = image::open(path)
            .map_err(|e| format!("Failed to load image '{}': {}", path.display(), e))?;

        let handle = self.create_from_image(device, queue, &img, options)?;
        self.path_to_handle.insert(path.to_path_buf(), handle);

        log::info!("Loaded texture: {} ({}x{})", path.display(), img.width(), img.height());
        Ok(handle)
    }

    /// Load a texture from raw image bytes
    pub fn load_from_bytes(
        &mut self,
        device: &Device,
        queue: &Queue,
        bytes: &[u8],
        options: &TextureOptions,
    ) -> Result<TextureHandle, String> {
        let img = image::load_from_memory(bytes)
            .map_err(|e| format!("Failed to decode image: {}", e))?;

        self.create_from_image(device, queue, &img, options)
    }

    /// Load a texture from raw RGBA pixel data
    pub fn load_from_rgba(
        &mut self,
        device: &Device,
        queue: &Queue,
        data: &[u8],
        width: u32,
        height: u32,
        options: &TextureOptions,
    ) -> Result<TextureHandle, String> {
        if data.len() != (width * height * 4) as usize {
            return Err(format!(
                "Invalid RGBA data size: expected {} bytes, got {}",
                width * height * 4,
                data.len()
            ));
        }

        // Create image from raw RGBA data
        let img = image::RgbaImage::from_raw(width, height, data.to_vec())
            .ok_or_else(|| "Failed to create image from raw RGBA data".to_string())?;

        self.create_from_image(device, queue, &DynamicImage::ImageRgba8(img), options)
    }

    /// Load an HDR environment map from a file
    /// Returns a Rgba16Float texture suitable for environment mapping
    pub fn load_hdr_from_file(
        &mut self,
        device: &Device,
        queue: &Queue,
        path: &Path,
    ) -> Result<TextureHandle, String> {
        // Check cache first
        if let Some(&handle) = self.path_to_handle.get(path) {
            return Ok(handle);
        }

        // Load HDR image using image crate (handles format detection)
        let img = image::open(path)
            .map_err(|e| format!("Failed to load HDR '{}': {}", path.display(), e))?;

        let (width, height) = img.dimensions();

        // Convert to Rgb32F for HDR data preservation
        let rgb32f = img.into_rgb32f();
        let rgb_data = rgb32f.as_raw();

        // Convert RGB f32 to RGBA f16 for GPU
        // Using half-precision for memory efficiency while preserving HDR range
        let pixel_count = (width * height) as usize;
        let mut rgba_f16: Vec<u16> = Vec::with_capacity(pixel_count * 4);
        for i in 0..pixel_count {
            let idx = i * 3;
            rgba_f16.push(f32_to_f16(rgb_data[idx]));
            rgba_f16.push(f32_to_f16(rgb_data[idx + 1]));
            rgba_f16.push(f32_to_f16(rgb_data[idx + 2]));
            rgba_f16.push(f32_to_f16(1.0)); // Alpha = 1.0
        }

        let format = TextureFormat::Rgba16Float;

        let texture = device.create_texture(&TextureDescriptor {
            label: Some("hdr_environment_texture"),
            size: Extent3d {
                width,
                height,
                depth_or_array_layers: 1,
            },
            mip_level_count: 1,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format,
            usage: TextureUsages::TEXTURE_BINDING | TextureUsages::COPY_DST,
            view_formats: &[],
        });

        // Upload HDR data
        queue.write_texture(
            ImageCopyTexture {
                texture: &texture,
                mip_level: 0,
                origin: Origin3d::ZERO,
                aspect: TextureAspect::All,
            },
            bytemuck::cast_slice(&rgba_f16),
            ImageDataLayout {
                offset: 0,
                bytes_per_row: Some(8 * width), // 4 channels * 2 bytes per f16
                rows_per_image: Some(height),
            },
            Extent3d {
                width,
                height,
                depth_or_array_layers: 1,
            },
        );

        let view = texture.create_view(&TextureViewDescriptor::default());

        // Sampler for equirectangular environment map
        let sampler = device.create_sampler(&SamplerDescriptor {
            label: Some("hdr_env_sampler"),
            address_mode_u: AddressMode::Repeat,
            address_mode_v: AddressMode::ClampToEdge,
            address_mode_w: AddressMode::Repeat,
            mag_filter: FilterMode::Linear,
            min_filter: FilterMode::Linear,
            mipmap_filter: FilterMode::Linear,
            ..Default::default()
        });

        let handle = TextureHandle(self.next_handle);
        self.next_handle += 1;

        self.textures.insert(handle, GpuTexture {
            texture,
            view,
            sampler,
            size: (width, height),
            format,
        });

        self.path_to_handle.insert(path.to_path_buf(), handle);

        log::info!("Loaded HDR environment: {} ({}x{})", path.display(), width, height);
        Ok(handle)
    }

    /// Create a texture from a DynamicImage
    fn create_from_image(
        &mut self,
        device: &Device,
        queue: &Queue,
        img: &DynamicImage,
        options: &TextureOptions,
    ) -> Result<TextureHandle, String> {
        let (width, height) = img.dimensions();

        // Convert to RGBA8
        let rgba = img.to_rgba8();
        let data = rgba.as_raw();

        // Determine format
        let format = options.format.unwrap_or_else(|| {
            if options.is_linear || options.is_normal_map {
                TextureFormat::Rgba8Unorm
            } else {
                TextureFormat::Rgba8UnormSrgb
            }
        });

        // Calculate mip levels
        // NOTE: Mipmaps disabled for now - we only upload mip level 0,
        // so other levels would be uninitialized (black)
        // TODO: Implement mipmap generation on GPU
        let mip_level_count = 1u32;

        let texture = device.create_texture(&TextureDescriptor {
            label: Some("loaded_texture"),
            size: Extent3d {
                width,
                height,
                depth_or_array_layers: 1,
            },
            mip_level_count,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format,
            usage: TextureUsages::TEXTURE_BINDING | TextureUsages::COPY_DST | TextureUsages::RENDER_ATTACHMENT,
            view_formats: &[],
        });

        // Upload base mip level
        queue.write_texture(
            ImageCopyTexture {
                texture: &texture,
                mip_level: 0,
                origin: Origin3d::ZERO,
                aspect: TextureAspect::All,
            },
            data,
            ImageDataLayout {
                offset: 0,
                bytes_per_row: Some(4 * width),
                rows_per_image: Some(height),
            },
            Extent3d {
                width,
                height,
                depth_or_array_layers: 1,
            },
        );

        // TODO: Generate mipmaps on GPU if needed
        // For now, mipmaps are not generated (would require compute shader or blit)

        let view = texture.create_view(&TextureViewDescriptor::default());

        let sampler = device.create_sampler(&SamplerDescriptor {
            label: Some("texture_sampler"),
            address_mode_u: options.address_mode,
            address_mode_v: options.address_mode,
            address_mode_w: options.address_mode,
            mag_filter: options.filter_mode,
            min_filter: options.filter_mode,
            mipmap_filter: if options.generate_mipmaps {
                FilterMode::Linear
            } else {
                FilterMode::Nearest
            },
            lod_min_clamp: 0.0,
            lod_max_clamp: mip_level_count as f32,
            compare: None,
            anisotropy_clamp: 16,
            border_color: None,
        });

        let handle = TextureHandle(self.next_handle);
        self.next_handle += 1;

        self.textures.insert(handle, GpuTexture {
            texture,
            view,
            sampler,
            size: (width, height),
            format,
        });

        Ok(handle)
    }

    /// Get a loaded texture by handle
    pub fn get(&self, handle: TextureHandle) -> Option<&GpuTexture> {
        self.textures.get(&handle)
    }

    /// Get texture view and sampler for binding
    pub fn get_binding(&self, handle: TextureHandle) -> Option<(&TextureView, &Sampler)> {
        self.textures.get(&handle).map(|t| (&t.view, &t.sampler))
    }

    /// Get texture view, falling back to default if handle is invalid
    pub fn get_view_or_default(&self, handle: TextureHandle, default: TextureHandle) -> &TextureView {
        self.textures
            .get(&handle)
            .or_else(|| self.textures.get(&default))
            .map(|t| &t.view)
            .expect("Default texture should always exist")
    }

    /// Get sampler, falling back to default if handle is invalid
    pub fn get_sampler_or_default(&self, handle: TextureHandle, default: TextureHandle) -> &Sampler {
        self.textures
            .get(&handle)
            .or_else(|| self.textures.get(&default))
            .map(|t| &t.sampler)
            .expect("Default texture should always exist")
    }

    /// Unload a texture
    pub fn unload(&mut self, handle: TextureHandle) {
        self.textures.remove(&handle);
        self.path_to_handle.retain(|_, &mut h| h != handle);
    }

    /// Get texture count
    pub fn texture_count(&self) -> usize {
        self.textures.len()
    }
}

/// PBR Material textures
#[derive(Clone, Debug, Default)]
pub struct PbrTextures {
    /// Base color / albedo texture
    pub albedo: TextureHandle,
    /// Normal map
    pub normal: TextureHandle,
    /// Metallic-roughness (G=roughness, B=metallic)
    pub metallic_roughness: TextureHandle,
    /// Ambient occlusion
    pub ao: TextureHandle,
    /// Emissive
    pub emissive: TextureHandle,
}

impl PbrTextures {
    pub fn new() -> Self {
        Self {
            albedo: TextureHandle::INVALID,
            normal: TextureHandle::INVALID,
            metallic_roughness: TextureHandle::INVALID,
            ao: TextureHandle::INVALID,
            emissive: TextureHandle::INVALID,
        }
    }

    /// Get texture flags for shader
    pub fn get_flags(&self) -> u32 {
        let mut flags = 0u32;
        if self.albedo.is_valid() { flags |= 1; }
        if self.normal.is_valid() { flags |= 2; }
        if self.metallic_roughness.is_valid() { flags |= 4; }
        if self.ao.is_valid() { flags |= 8; }
        if self.emissive.is_valid() { flags |= 16; }
        flags
    }
}

/// Convert f32 to f16 (IEEE 754 half-precision)
fn f32_to_f16(value: f32) -> u16 {
    let bits = value.to_bits();
    let sign = (bits >> 16) & 0x8000;
    let exp = ((bits >> 23) & 0xFF) as i32;
    let mantissa = bits & 0x7FFFFF;

    if exp == 255 {
        // Inf or NaN
        if mantissa != 0 {
            return (sign | 0x7E00) as u16; // NaN
        } else {
            return (sign | 0x7C00) as u16; // Inf
        }
    }

    let new_exp = exp - 127 + 15;

    if new_exp >= 31 {
        // Overflow to infinity
        return (sign | 0x7C00) as u16;
    }

    if new_exp <= 0 {
        // Denormalized or zero
        if new_exp < -10 {
            return sign as u16; // Too small, return zero
        }
        let mantissa = (mantissa | 0x800000) >> (1 - new_exp + 13);
        return (sign | mantissa) as u16;
    }

    let new_mantissa = mantissa >> 13;
    (sign | ((new_exp as u32) << 10) | new_mantissa) as u16
}
