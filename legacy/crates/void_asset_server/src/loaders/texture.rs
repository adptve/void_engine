//! Texture loader for PNG, JPG, BMP images

use image::{DynamicImage, GenericImageView};

/// Texture data ready for GPU upload
#[derive(Clone, Debug)]
pub struct TextureAsset {
    /// Raw RGBA pixel data
    pub data: Vec<u8>,
    /// Width in pixels
    pub width: u32,
    /// Height in pixels
    pub height: u32,
    /// Bytes per row (width * 4 for RGBA)
    pub bytes_per_row: u32,
    /// Whether texture uses sRGB color space
    pub srgb: bool,
    /// Optional mipmap levels (level 0 is base)
    pub mips: Vec<Vec<u8>>,
}

/// Loader for image textures
pub struct TextureLoader {
    /// Generate mipmaps on load
    pub generate_mips: bool,
    /// Interpret as sRGB
    pub srgb: bool,
}

impl Default for TextureLoader {
    fn default() -> Self {
        Self {
            generate_mips: false, // Keep simple for now
            srgb: true,
        }
    }
}

impl TextureLoader {
    /// Create a new texture loader
    pub fn new() -> Self {
        Self::default()
    }

    /// Load a texture from image bytes
    pub fn load(&self, data: &[u8], path: &str) -> Result<TextureAsset, String> {
        let img = image::load_from_memory(data)
            .map_err(|e| format!("Failed to decode image {}: {}", path, e))?;

        let rgba = img.to_rgba8();
        let (width, height) = rgba.dimensions();
        let bytes_per_row = width * 4;

        // wgpu requires rows to be aligned to 256 bytes for some operations
        // but for texture data upload we use the actual bytes_per_row

        let mips = if self.generate_mips {
            Self::generate_mip_chain(&img)
        } else {
            vec![]
        };

        Ok(TextureAsset {
            data: rgba.into_raw(),
            width,
            height,
            bytes_per_row,
            srgb: self.srgb,
            mips,
        })
    }

    /// Generate mipmap chain from image
    fn generate_mip_chain(img: &DynamicImage) -> Vec<Vec<u8>> {
        let mut mips = Vec::new();
        let (mut w, mut h) = img.dimensions();
        let mut current = img.clone();

        while w > 1 || h > 1 {
            w = (w / 2).max(1);
            h = (h / 2).max(1);

            current = current.resize_exact(w, h, image::imageops::FilterType::Lanczos3);
            mips.push(current.to_rgba8().into_raw());
        }

        mips
    }

    /// Create a 1x1 solid color texture (useful for defaults)
    pub fn solid_color(r: u8, g: u8, b: u8, a: u8) -> TextureAsset {
        TextureAsset {
            data: vec![r, g, b, a],
            width: 1,
            height: 1,
            bytes_per_row: 4,
            srgb: true,
            mips: vec![],
        }
    }

    /// Create a checkerboard pattern texture
    pub fn checkerboard(size: u32, tile_size: u32, color1: [u8; 4], color2: [u8; 4]) -> TextureAsset {
        let mut data = Vec::with_capacity((size * size * 4) as usize);

        for y in 0..size {
            for x in 0..size {
                let tx = x / tile_size;
                let ty = y / tile_size;
                let color = if (tx + ty) % 2 == 0 { color1 } else { color2 };
                data.extend_from_slice(&color);
            }
        }

        TextureAsset {
            data,
            width: size,
            height: size,
            bytes_per_row: size * 4,
            srgb: true,
            mips: vec![],
        }
    }

    /// Create a grid pattern texture
    pub fn grid(size: u32, line_width: u32, bg_color: [u8; 4], line_color: [u8; 4]) -> TextureAsset {
        let mut data = Vec::with_capacity((size * size * 4) as usize);
        let cell_size = size / 8; // 8x8 grid

        for y in 0..size {
            for x in 0..size {
                let on_line = (x % cell_size) < line_width || (y % cell_size) < line_width;
                let color = if on_line { line_color } else { bg_color };
                data.extend_from_slice(&color);
            }
        }

        TextureAsset {
            data,
            width: size,
            height: size,
            bytes_per_row: size * 4,
            srgb: true,
            mips: vec![],
        }
    }

    /// Create a UV test pattern texture (red = U, green = V)
    pub fn uv_test(size: u32) -> TextureAsset {
        let mut data = Vec::with_capacity((size * size * 4) as usize);

        for y in 0..size {
            for x in 0..size {
                let u = (x as f32 / size as f32 * 255.0) as u8;
                let v = (y as f32 / size as f32 * 255.0) as u8;
                data.extend_from_slice(&[u, v, 128, 255]);
            }
        }

        TextureAsset {
            data,
            width: size,
            height: size,
            bytes_per_row: size * 4,
            srgb: false, // UV test should be linear
            mips: vec![],
        }
    }
}
