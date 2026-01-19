//! High Dynamic Range (HDR) support
//!
//! This module handles HDR detection, configuration, and metadata management.
//! Supports HDR10, HLG, and wide color gamut displays.

/// HDR configuration
#[derive(Debug, Clone)]
pub struct HdrConfig {
    /// Is HDR enabled?
    pub enabled: bool,
    /// Transfer function (tone curve)
    pub transfer_function: TransferFunction,
    /// Color primaries
    pub color_primaries: ColorPrimaries,
    /// Maximum luminance in nits
    pub max_luminance: u32,
    /// Minimum luminance in nits
    pub min_luminance: f32,
    /// Maximum content light level (MaxCLL) in nits
    pub max_content_light_level: Option<u32>,
    /// Maximum frame average light level (MaxFALL) in nits
    pub max_frame_average_light_level: Option<u32>,
}

/// Transfer function (EOTF - Electro-Optical Transfer Function)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TransferFunction {
    /// Standard Dynamic Range (sRGB/Rec.709)
    Sdr,
    /// Perceptual Quantizer (HDR10, HDR10+)
    Pq,
    /// Hybrid Log-Gamma (HLG broadcast)
    Hlg,
    /// Linear (for intermediate processing)
    Linear,
}

impl TransferFunction {
    /// Get the SMPTE ST 2084 EOTF ID
    pub fn eotf_id(&self) -> u8 {
        match self {
            Self::Sdr => 0,      // Traditional gamma (SDR)
            Self::Pq => 2,       // SMPTE ST 2084 (PQ)
            Self::Hlg => 3,      // ARIB STD-B67 (HLG)
            Self::Linear => 1,   // Linear
        }
    }

    /// Get human-readable name
    pub fn name(&self) -> &'static str {
        match self {
            Self::Sdr => "SDR",
            Self::Pq => "PQ (HDR10)",
            Self::Hlg => "HLG",
            Self::Linear => "Linear",
        }
    }

    /// Check if this is an HDR transfer function
    pub fn is_hdr(&self) -> bool {
        matches!(self, Self::Pq | Self::Hlg)
    }
}

/// Color primaries (color gamut)
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ColorPrimaries {
    /// sRGB / Rec.709 (SDR standard)
    Srgb,
    /// DCI-P3 (digital cinema, common in HDR displays)
    DciP3,
    /// Rec.2020 (ultra-wide gamut, HDR standard)
    Rec2020,
    /// Adobe RGB (photography)
    AdobeRgb,
}

impl ColorPrimaries {
    /// Get primaries as CIE 1931 xy coordinates
    ///
    /// Returns: (red_x, red_y, green_x, green_y, blue_x, blue_y, white_x, white_y)
    pub fn as_cie_xy(&self) -> (f32, f32, f32, f32, f32, f32, f32, f32) {
        match self {
            Self::Srgb => (
                0.640, 0.330, // Red
                0.300, 0.600, // Green
                0.150, 0.060, // Blue
                0.3127, 0.3290, // White point (D65)
            ),
            Self::DciP3 => (
                0.680, 0.320, // Red
                0.265, 0.690, // Green
                0.150, 0.060, // Blue
                0.3127, 0.3290, // White point (D65)
            ),
            Self::Rec2020 => (
                0.708, 0.292, // Red
                0.170, 0.797, // Green
                0.131, 0.046, // Blue
                0.3127, 0.3290, // White point (D65)
            ),
            Self::AdobeRgb => (
                0.640, 0.330, // Red
                0.210, 0.710, // Green
                0.150, 0.060, // Blue
                0.3127, 0.3290, // White point (D65)
            ),
        }
    }

    /// Get human-readable name
    pub fn name(&self) -> &'static str {
        match self {
            Self::Srgb => "sRGB/Rec.709",
            Self::DciP3 => "DCI-P3",
            Self::Rec2020 => "Rec.2020",
            Self::AdobeRgb => "Adobe RGB",
        }
    }

    /// Get the color space ID (for DRM metadata)
    pub fn color_space_id(&self) -> u8 {
        match self {
            Self::Srgb => 0,        // Default
            Self::DciP3 => 1,       // DCI-P3
            Self::Rec2020 => 2,     // Rec.2020
            Self::AdobeRgb => 3,    // Adobe RGB
        }
    }
}

impl Default for HdrConfig {
    fn default() -> Self {
        Self {
            enabled: false,
            transfer_function: TransferFunction::Sdr,
            color_primaries: ColorPrimaries::Srgb,
            max_luminance: 100,      // SDR default
            min_luminance: 0.0,
            max_content_light_level: None,
            max_frame_average_light_level: None,
        }
    }
}

impl HdrConfig {
    /// Create an HDR10 configuration
    pub fn hdr10(max_nits: u32) -> Self {
        Self {
            enabled: true,
            transfer_function: TransferFunction::Pq,
            color_primaries: ColorPrimaries::Rec2020,
            max_luminance: max_nits,
            min_luminance: 0.0001,
            max_content_light_level: Some(max_nits),
            max_frame_average_light_level: Some(max_nits / 2),
        }
    }

    /// Create an HLG configuration
    pub fn hlg(max_nits: u32) -> Self {
        Self {
            enabled: true,
            transfer_function: TransferFunction::Hlg,
            color_primaries: ColorPrimaries::Rec2020,
            max_luminance: max_nits,
            min_luminance: 0.0,
            max_content_light_level: None, // HLG doesn't use static metadata
            max_frame_average_light_level: None,
        }
    }

    /// Create an SDR configuration
    pub fn sdr() -> Self {
        Self::default()
    }

    /// Enable HDR with the given transfer function
    pub fn enable(&mut self, transfer_function: TransferFunction) {
        self.enabled = true;
        self.transfer_function = transfer_function;

        // Set appropriate color primaries for HDR
        if transfer_function.is_hdr() {
            self.color_primaries = ColorPrimaries::Rec2020;
        }
    }

    /// Disable HDR (return to SDR)
    pub fn disable(&mut self) {
        self.enabled = false;
        self.transfer_function = TransferFunction::Sdr;
        self.color_primaries = ColorPrimaries::Srgb;
    }

    /// Check if HDR is active
    pub fn is_active(&self) -> bool {
        self.enabled && self.transfer_function.is_hdr()
    }

    /// Get the nits-per-stop for exposure calculations
    pub fn nits_per_stop(&self) -> f32 {
        if self.is_active() {
            // HDR: wider range
            self.max_luminance as f32 / 10.0
        } else {
            // SDR: standard range
            100.0 / 10.0
        }
    }

    /// Convert to DRM HDR metadata blob
    ///
    /// This creates the metadata structure expected by the kernel.
    /// Format: HDR Static Metadata Type 1 (SMPTE ST 2086 + CTA-861-G)
    pub fn to_drm_metadata(&self) -> HdrMetadata {
        let (rx, ry, gx, gy, bx, by, wx, wy) = self.color_primaries.as_cie_xy();

        HdrMetadata {
            // Display primaries (x and y for R, G, B)
            display_primaries_x: [
                (rx * 50000.0) as u16,
                (gx * 50000.0) as u16,
                (bx * 50000.0) as u16,
            ],
            display_primaries_y: [
                (ry * 50000.0) as u16,
                (gy * 50000.0) as u16,
                (by * 50000.0) as u16,
            ],
            // White point
            white_point_x: (wx * 50000.0) as u16,
            white_point_y: (wy * 50000.0) as u16,
            // Luminance range
            max_display_mastering_luminance: self.max_luminance,
            min_display_mastering_luminance: (self.min_luminance * 10000.0) as u32,
            // Content light levels
            max_content_light_level: self.max_content_light_level.unwrap_or(0),
            max_frame_average_light_level: self.max_frame_average_light_level.unwrap_or(0),
            // EOTF
            eotf: self.transfer_function.eotf_id(),
        }
    }
}

/// HDR metadata structure (matches kernel DRM hdr_output_metadata)
#[derive(Debug, Clone, Copy)]
#[repr(C)]
pub struct HdrMetadata {
    /// Display primaries (red, green, blue) - x coordinates
    /// Values are in units of 0.00002
    pub display_primaries_x: [u16; 3],
    /// Display primaries (red, green, blue) - y coordinates
    pub display_primaries_y: [u16; 3],
    /// White point x coordinate
    pub white_point_x: u16,
    /// White point y coordinate
    pub white_point_y: u16,
    /// Maximum display mastering luminance (nits)
    pub max_display_mastering_luminance: u32,
    /// Minimum display mastering luminance (0.0001 nits)
    pub min_display_mastering_luminance: u32,
    /// Maximum content light level (nits)
    pub max_content_light_level: u32,
    /// Maximum frame-average light level (nits)
    pub max_frame_average_light_level: u32,
    /// EOTF (transfer function)
    pub eotf: u8,
}

/// HDR capability detection result
#[derive(Debug, Clone)]
pub struct HdrCapability {
    /// Is HDR supported?
    pub supported: bool,
    /// Supported transfer functions
    pub transfer_functions: Vec<TransferFunction>,
    /// Maximum luminance in nits
    pub max_luminance: Option<u32>,
    /// Minimum luminance in nits
    pub min_luminance: Option<f32>,
    /// Supported color gamuts
    pub color_gamuts: Vec<ColorPrimaries>,
}

impl Default for HdrCapability {
    fn default() -> Self {
        Self {
            supported: false,
            transfer_functions: vec![TransferFunction::Sdr],
            max_luminance: Some(100), // SDR default
            min_luminance: Some(0.0),
            color_gamuts: vec![ColorPrimaries::Srgb],
        }
    }
}

impl HdrCapability {
    /// Create a capability for an HDR10-capable display
    pub fn hdr10(max_nits: u32, min_nits: f32) -> Self {
        Self {
            supported: true,
            transfer_functions: vec![TransferFunction::Sdr, TransferFunction::Pq],
            max_luminance: Some(max_nits),
            min_luminance: Some(min_nits),
            color_gamuts: vec![ColorPrimaries::Srgb, ColorPrimaries::DciP3, ColorPrimaries::Rec2020],
        }
    }

    /// Create a capability for an HLG-capable display
    pub fn hlg(max_nits: u32) -> Self {
        Self {
            supported: true,
            transfer_functions: vec![TransferFunction::Sdr, TransferFunction::Hlg],
            max_luminance: Some(max_nits),
            min_luminance: Some(0.0),
            color_gamuts: vec![ColorPrimaries::Srgb, ColorPrimaries::Rec2020],
        }
    }

    /// Create a capability for a non-HDR display
    pub fn sdr_only() -> Self {
        Self::default()
    }

    /// Check if a specific transfer function is supported
    pub fn supports_transfer_function(&self, tf: TransferFunction) -> bool {
        self.transfer_functions.contains(&tf)
    }

    /// Check if a specific color gamut is supported
    pub fn supports_color_gamut(&self, gamut: ColorPrimaries) -> bool {
        self.color_gamuts.contains(&gamut)
    }

    /// Convert to HdrConfig (if HDR is supported)
    pub fn to_config(&self, prefer_hdr10: bool) -> HdrConfig {
        if !self.supported {
            return HdrConfig::sdr();
        }

        let max_nits = self.max_luminance.unwrap_or(1000);

        if prefer_hdr10 && self.supports_transfer_function(TransferFunction::Pq) {
            HdrConfig::hdr10(max_nits)
        } else if self.supports_transfer_function(TransferFunction::Hlg) {
            HdrConfig::hlg(max_nits)
        } else {
            HdrConfig::sdr()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hdr_config_hdr10() {
        let config = HdrConfig::hdr10(1000);
        assert!(config.is_active());
        assert_eq!(config.transfer_function, TransferFunction::Pq);
        assert_eq!(config.color_primaries, ColorPrimaries::Rec2020);
        assert_eq!(config.max_luminance, 1000);
    }

    #[test]
    fn test_hdr_config_hlg() {
        let config = HdrConfig::hlg(600);
        assert!(config.is_active());
        assert_eq!(config.transfer_function, TransferFunction::Hlg);
        assert_eq!(config.max_luminance, 600);
    }

    #[test]
    fn test_hdr_enable_disable() {
        let mut config = HdrConfig::sdr();
        assert!(!config.is_active());

        config.enable(TransferFunction::Pq);
        assert!(config.is_active());
        assert_eq!(config.color_primaries, ColorPrimaries::Rec2020);

        config.disable();
        assert!(!config.is_active());
        assert_eq!(config.transfer_function, TransferFunction::Sdr);
    }

    #[test]
    fn test_transfer_function_eotf() {
        assert_eq!(TransferFunction::Sdr.eotf_id(), 0);
        assert_eq!(TransferFunction::Pq.eotf_id(), 2);
        assert_eq!(TransferFunction::Hlg.eotf_id(), 3);
        assert_eq!(TransferFunction::Linear.eotf_id(), 1);
    }

    #[test]
    fn test_color_primaries() {
        let srgb = ColorPrimaries::Srgb.as_cie_xy();
        assert!((srgb.0 - 0.640).abs() < 0.001); // Red x

        let rec2020 = ColorPrimaries::Rec2020.as_cie_xy();
        assert!((rec2020.0 - 0.708).abs() < 0.001); // Red x
    }

    #[test]
    fn test_hdr_capability() {
        let cap = HdrCapability::hdr10(1000, 0.0001);
        assert!(cap.supported);
        assert!(cap.supports_transfer_function(TransferFunction::Pq));
        assert!(cap.supports_color_gamut(ColorPrimaries::Rec2020));

        let config = cap.to_config(true);
        assert!(config.is_active());
        assert_eq!(config.transfer_function, TransferFunction::Pq);
    }

    #[test]
    fn test_drm_metadata() {
        let config = HdrConfig::hdr10(1000);
        let metadata = config.to_drm_metadata();

        assert_eq!(metadata.eotf, 2); // PQ
        assert_eq!(metadata.max_display_mastering_luminance, 1000);
        assert!(metadata.max_content_light_level > 0);
    }
}
