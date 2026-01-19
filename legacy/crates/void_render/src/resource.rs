//! Render Resources - GPU resource abstractions
//!
//! Abstract representations of GPU resources that can be implemented
//! by any graphics backend (Vulkan, WGPU, DirectX, etc.)

use alloc::string::String;
use alloc::vec::Vec;
use core::hash::{Hash, Hasher};
use void_core::Id;

/// Unique identifier for a render resource
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct ResourceId(pub Id);

impl ResourceId {
    /// Create from a name
    pub fn from_name(name: &str) -> Self {
        Self(Id::from_name(name))
    }

    /// Get the raw ID
    pub fn id(&self) -> Id {
        self.0
    }
}

/// Texture format
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum TextureFormat {
    // 8-bit formats
    R8Unorm,
    R8Snorm,
    R8Uint,
    R8Sint,

    // 16-bit formats
    R16Uint,
    R16Sint,
    R16Float,
    Rg8Unorm,
    Rg8Snorm,
    Rg8Uint,
    Rg8Sint,

    // 32-bit formats
    R32Uint,
    R32Sint,
    R32Float,
    Rg16Uint,
    Rg16Sint,
    Rg16Float,
    Rgba8Unorm,
    Rgba8UnormSrgb,
    Rgba8Snorm,
    Rgba8Uint,
    Rgba8Sint,
    Bgra8Unorm,
    Bgra8UnormSrgb,

    // 64-bit formats
    Rg32Uint,
    Rg32Sint,
    Rg32Float,
    Rgba16Uint,
    Rgba16Sint,
    Rgba16Float,

    // 128-bit formats
    Rgba32Uint,
    Rgba32Sint,
    Rgba32Float,

    // Depth/stencil formats
    Depth16Unorm,
    Depth24Plus,
    Depth24PlusStencil8,
    Depth32Float,
    Depth32FloatStencil8,

    // Compressed formats
    Bc1RgbaUnorm,
    Bc1RgbaUnormSrgb,
    Bc2RgbaUnorm,
    Bc2RgbaUnormSrgb,
    Bc3RgbaUnorm,
    Bc3RgbaUnormSrgb,
    Bc4RUnorm,
    Bc4RSnorm,
    Bc5RgUnorm,
    Bc5RgSnorm,
    Bc6hRgbUfloat,
    Bc6hRgbFloat,
    Bc7RgbaUnorm,
    Bc7RgbaUnormSrgb,
}

impl TextureFormat {
    /// Check if this is a depth format
    pub fn is_depth(&self) -> bool {
        matches!(
            self,
            Self::Depth16Unorm
                | Self::Depth24Plus
                | Self::Depth24PlusStencil8
                | Self::Depth32Float
                | Self::Depth32FloatStencil8
        )
    }

    /// Check if this is a stencil format
    pub fn is_stencil(&self) -> bool {
        matches!(
            self,
            Self::Depth24PlusStencil8 | Self::Depth32FloatStencil8
        )
    }

    /// Check if this is sRGB
    pub fn is_srgb(&self) -> bool {
        matches!(
            self,
            Self::Rgba8UnormSrgb
                | Self::Bgra8UnormSrgb
                | Self::Bc1RgbaUnormSrgb
                | Self::Bc2RgbaUnormSrgb
                | Self::Bc3RgbaUnormSrgb
                | Self::Bc7RgbaUnormSrgb
        )
    }

    /// Bytes per pixel (0 for compressed)
    pub fn bytes_per_pixel(&self) -> usize {
        match self {
            Self::R8Unorm | Self::R8Snorm | Self::R8Uint | Self::R8Sint => 1,
            Self::R16Uint
            | Self::R16Sint
            | Self::R16Float
            | Self::Rg8Unorm
            | Self::Rg8Snorm
            | Self::Rg8Uint
            | Self::Rg8Sint
            | Self::Depth16Unorm => 2,
            Self::R32Uint
            | Self::R32Sint
            | Self::R32Float
            | Self::Rg16Uint
            | Self::Rg16Sint
            | Self::Rg16Float
            | Self::Rgba8Unorm
            | Self::Rgba8UnormSrgb
            | Self::Rgba8Snorm
            | Self::Rgba8Uint
            | Self::Rgba8Sint
            | Self::Bgra8Unorm
            | Self::Bgra8UnormSrgb
            | Self::Depth24Plus
            | Self::Depth24PlusStencil8
            | Self::Depth32Float => 4,
            Self::Rg32Uint
            | Self::Rg32Sint
            | Self::Rg32Float
            | Self::Rgba16Uint
            | Self::Rgba16Sint
            | Self::Rgba16Float
            | Self::Depth32FloatStencil8 => 8,
            Self::Rgba32Uint | Self::Rgba32Sint | Self::Rgba32Float => 16,
            _ => 0, // Compressed formats
        }
    }
}

/// Texture dimension
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum TextureDimension {
    D1,
    D2,
    D3,
}

/// Texture usage flags
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct TextureUsage(pub u32);

impl TextureUsage {
    pub const COPY_SRC: Self = Self(1 << 0);
    pub const COPY_DST: Self = Self(1 << 1);
    pub const TEXTURE_BINDING: Self = Self(1 << 2);
    pub const STORAGE_BINDING: Self = Self(1 << 3);
    pub const RENDER_ATTACHMENT: Self = Self(1 << 4);

    pub fn contains(&self, other: Self) -> bool {
        (self.0 & other.0) == other.0
    }

    pub fn union(self, other: Self) -> Self {
        Self(self.0 | other.0)
    }
}

impl core::ops::BitOr for TextureUsage {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

/// Texture descriptor
#[derive(Clone, Debug)]
pub struct TextureDesc {
    /// Debug label
    pub label: Option<String>,
    /// Size in pixels
    pub size: [u32; 3],
    /// Mip level count
    pub mip_level_count: u32,
    /// Sample count
    pub sample_count: u32,
    /// Dimension
    pub dimension: TextureDimension,
    /// Format
    pub format: TextureFormat,
    /// Usage flags
    pub usage: TextureUsage,
}

impl Default for TextureDesc {
    fn default() -> Self {
        Self {
            label: None,
            size: [1, 1, 1],
            mip_level_count: 1,
            sample_count: 1,
            dimension: TextureDimension::D2,
            format: TextureFormat::Rgba8Unorm,
            usage: TextureUsage::TEXTURE_BINDING,
        }
    }
}

/// Buffer usage flags
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub struct BufferUsage(pub u32);

impl BufferUsage {
    pub const MAP_READ: Self = Self(1 << 0);
    pub const MAP_WRITE: Self = Self(1 << 1);
    pub const COPY_SRC: Self = Self(1 << 2);
    pub const COPY_DST: Self = Self(1 << 3);
    pub const INDEX: Self = Self(1 << 4);
    pub const VERTEX: Self = Self(1 << 5);
    pub const UNIFORM: Self = Self(1 << 6);
    pub const STORAGE: Self = Self(1 << 7);
    pub const INDIRECT: Self = Self(1 << 8);

    pub fn contains(&self, other: Self) -> bool {
        (self.0 & other.0) == other.0
    }
}

impl core::ops::BitOr for BufferUsage {
    type Output = Self;
    fn bitor(self, rhs: Self) -> Self::Output {
        Self(self.0 | rhs.0)
    }
}

/// Buffer descriptor
#[derive(Clone, Debug)]
pub struct BufferDesc {
    /// Debug label
    pub label: Option<String>,
    /// Size in bytes
    pub size: u64,
    /// Usage flags
    pub usage: BufferUsage,
    /// Mapped at creation
    pub mapped_at_creation: bool,
}

impl Default for BufferDesc {
    fn default() -> Self {
        Self {
            label: None,
            size: 0,
            usage: BufferUsage::COPY_DST,
            mapped_at_creation: false,
        }
    }
}

/// Sampler filter mode
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum FilterMode {
    Nearest,
    Linear,
}

/// Sampler address mode
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum AddressMode {
    ClampToEdge,
    Repeat,
    MirrorRepeat,
    ClampToBorder,
}

/// Compare function
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum CompareFunction {
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
}

/// Sampler descriptor
#[derive(Clone, Debug)]
pub struct SamplerDesc {
    /// Debug label
    pub label: Option<String>,
    /// Address mode U
    pub address_mode_u: AddressMode,
    /// Address mode V
    pub address_mode_v: AddressMode,
    /// Address mode W
    pub address_mode_w: AddressMode,
    /// Magnification filter
    pub mag_filter: FilterMode,
    /// Minification filter
    pub min_filter: FilterMode,
    /// Mipmap filter
    pub mipmap_filter: FilterMode,
    /// LOD clamp minimum
    pub lod_min_clamp: f32,
    /// LOD clamp maximum
    pub lod_max_clamp: f32,
    /// Compare function for depth samplers
    pub compare: Option<CompareFunction>,
    /// Anisotropy clamp
    pub anisotropy_clamp: u16,
}

impl Default for SamplerDesc {
    fn default() -> Self {
        Self {
            label: None,
            address_mode_u: AddressMode::ClampToEdge,
            address_mode_v: AddressMode::ClampToEdge,
            address_mode_w: AddressMode::ClampToEdge,
            mag_filter: FilterMode::Linear,
            min_filter: FilterMode::Linear,
            mipmap_filter: FilterMode::Linear,
            lod_min_clamp: 0.0,
            lod_max_clamp: 32.0,
            compare: None,
            anisotropy_clamp: 1,
        }
    }
}

/// Framebuffer attachment description
#[derive(Clone, Debug)]
pub struct AttachmentDesc {
    /// Texture format
    pub format: TextureFormat,
    /// Sample count
    pub samples: u32,
    /// Load operation
    pub load_op: LoadOp,
    /// Store operation
    pub store_op: StoreOp,
}

/// Load operation for attachments
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LoadOp {
    /// Clear to a value
    Clear,
    /// Load existing contents
    Load,
    /// Don't care (undefined)
    DontCare,
}

/// Store operation for attachments
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum StoreOp {
    /// Store the result
    Store,
    /// Discard the result
    Discard,
}

/// Clear value for attachments
#[derive(Clone, Copy, Debug)]
pub enum ClearValue {
    /// Color clear value (RGBA)
    Color([f32; 4]),
    /// Depth clear value
    Depth(f32),
    /// Stencil clear value
    Stencil(u32),
    /// Depth and stencil clear value
    DepthStencil(f32, u32),
}

impl Default for ClearValue {
    fn default() -> Self {
        Self::Color([0.0, 0.0, 0.0, 1.0])
    }
}
