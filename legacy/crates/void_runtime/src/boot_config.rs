//! Boot Configuration
//!
//! Handles backend selection at boot time. The boot configuration determines
//! which presenter/compositor backend to use:
//!
//! - **Smithay (default on Linux)**: DRM/KMS + libinput, Wayland compositor
//! - **winit (development)**: Window on existing display server
//! - **XR**: OpenXR for VR/AR headsets
//! - **CLI**: No GUI, shell-only mode
//!
//! # Configuration Sources (in priority order)
//!
//! 1. Kernel command line: `metaverse.backend=smithay`
//! 2. Environment variable: `METAVERSE_BACKEND=winit`
//! 3. Config file: `/etc/metaverse/boot.conf`
//! 4. Auto-detection based on available hardware
//!
//! # Example Config File
//!
//! ```toml
//! [boot]
//! backend = "smithay"  # smithay, winit, xr, cli
//! fallback = "cli"     # fallback if primary fails
//!
//! [display]
//! resolution = "1920x1080"
//! refresh_rate = 60
//! vrr = true
//! hdr = false
//!
//! [xr]
//! enabled = true
//! mode = "vr"  # vr, ar, mr, desktop
//! supersampling = 1.2
//! ```

use std::path::Path;
use serde::{Deserialize, Serialize};

/// Backend type for display output
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Backend {
    /// Smithay compositor (DRM/KMS, libinput, Wayland)
    /// This is the production backend for running as an OS
    Smithay,

    /// winit window (runs on existing display server)
    /// This is for development/testing on desktop
    Winit,

    /// OpenXR for VR/AR headsets
    Xr,

    /// CLI only, no GUI
    Cli,

    /// Auto-detect best backend
    Auto,
}

impl Default for Backend {
    fn default() -> Self {
        Self::Auto
    }
}

impl std::fmt::Display for Backend {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Smithay => write!(f, "smithay"),
            Self::Winit => write!(f, "winit"),
            Self::Xr => write!(f, "xr"),
            Self::Cli => write!(f, "cli"),
            Self::Auto => write!(f, "auto"),
        }
    }
}

impl std::str::FromStr for Backend {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_lowercase().as_str() {
            "smithay" | "drm" | "wayland" => Ok(Self::Smithay),
            "winit" | "window" | "desktop" => Ok(Self::Winit),
            "xr" | "vr" | "ar" | "openxr" => Ok(Self::Xr),
            "cli" | "shell" | "none" => Ok(Self::Cli),
            "auto" | "" => Ok(Self::Auto),
            _ => Err(format!("Unknown backend: {}", s)),
        }
    }
}

/// Display configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DisplayConfig {
    /// Resolution (width x height)
    pub resolution: Option<(u32, u32)>,
    /// Refresh rate in Hz
    pub refresh_rate: Option<u32>,
    /// Enable Variable Refresh Rate
    pub vrr: bool,
    /// Enable HDR
    pub hdr: bool,
    /// Scale factor
    pub scale: f32,
}

impl Default for DisplayConfig {
    fn default() -> Self {
        Self {
            resolution: None, // Auto-detect
            refresh_rate: None, // Auto-detect
            vrr: true,
            hdr: false,
            scale: 1.0,
        }
    }
}

/// XR configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct XrConfig {
    /// Enable XR support
    pub enabled: bool,
    /// XR mode (vr, ar, mr, desktop)
    pub mode: String,
    /// Supersampling factor
    pub supersampling: f32,
    /// Enable hand tracking
    pub hand_tracking: bool,
    /// Enable passthrough (AR mode)
    pub passthrough: bool,
}

impl Default for XrConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            mode: "vr".to_string(),
            supersampling: 1.0,
            hand_tracking: true,
            passthrough: false,
        }
    }
}

/// Complete boot configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BootConfig {
    /// Primary backend to use
    pub backend: Backend,
    /// Fallback backend if primary fails
    pub fallback: Backend,
    /// Display configuration
    pub display: DisplayConfig,
    /// XR configuration
    pub xr: XrConfig,
    /// Enable debug logging
    pub debug: bool,
    /// App to load on startup (path to app directory)
    pub startup_app: Option<String>,
    /// Config file path (for reloading)
    #[serde(skip)]
    pub config_path: Option<String>,
}

impl Default for BootConfig {
    fn default() -> Self {
        Self {
            backend: Backend::Auto,
            fallback: Backend::Cli,
            display: DisplayConfig::default(),
            xr: XrConfig::default(),
            debug: false,
            startup_app: None,
            config_path: None,
        }
    }
}

impl BootConfig {
    /// Load boot configuration from all sources
    pub fn load() -> Self {
        let mut config = Self::default();

        // 1. Try config file first
        for path in &[
            "/etc/metaverse/boot.conf",
            "/etc/metaverse/boot.toml",
            "boot.conf",
            "boot.toml",
        ] {
            if let Ok(loaded) = Self::load_from_file(path) {
                config = loaded;
                config.config_path = Some(path.to_string());
                log::info!("Loaded boot config from {}", path);
                break;
            }
        }

        // 2. Override with environment variables
        if let Ok(backend) = std::env::var("METAVERSE_BACKEND") {
            if let Ok(b) = backend.parse() {
                config.backend = b;
                log::info!("Backend from env: {}", config.backend);
            }
        }

        if let Ok(fallback) = std::env::var("METAVERSE_FALLBACK") {
            if let Ok(b) = fallback.parse() {
                config.fallback = b;
            }
        }

        if std::env::var("METAVERSE_DEBUG").is_ok() {
            config.debug = true;
        }

        if let Ok(app) = std::env::var("METAVERSE_APP") {
            if !app.is_empty() {
                config.startup_app = Some(app);
                log::info!("Startup app from env: {:?}", config.startup_app);
            }
        }

        // Parse command line arguments for startup app (positional argument)
        let args: Vec<String> = std::env::args().collect();
        for arg in args.iter().skip(1) {
            // Skip flags starting with --
            if arg.starts_with("--") {
                continue;
            }
            // First non-flag argument is the startup app path
            if config.startup_app.is_none() {
                config.startup_app = Some(arg.clone());
                log::info!("Startup app from args: {:?}", config.startup_app);
                break;
            }
        }

        if std::env::var("METAVERSE_VRR").map(|v| v == "1" || v == "true").unwrap_or(false) {
            config.display.vrr = true;
        }

        if std::env::var("METAVERSE_HDR").map(|v| v == "1" || v == "true").unwrap_or(false) {
            config.display.hdr = true;
        }

        // 3. Override with kernel command line (Linux)
        #[cfg(target_os = "linux")]
        {
            if let Ok(cmdline) = std::fs::read_to_string("/proc/cmdline") {
                for param in cmdline.split_whitespace() {
                    if let Some(value) = param.strip_prefix("metaverse.backend=") {
                        if let Ok(b) = value.parse() {
                            config.backend = b;
                            log::info!("Backend from kernel cmdline: {}", config.backend);
                        }
                    }
                    if param == "metaverse.debug" {
                        config.debug = true;
                    }
                }
            }
        }

        // 4. Auto-detect if backend is Auto
        if config.backend == Backend::Auto {
            config.backend = Self::detect_best_backend();
            log::info!("Auto-detected backend: {}", config.backend);
        }

        config
    }

    /// Load configuration from a TOML file
    fn load_from_file(path: &str) -> Result<Self, Box<dyn std::error::Error>> {
        let content = std::fs::read_to_string(path)?;

        // Simple TOML-like parser (avoid external dependency)
        let mut config = Self::default();

        for line in content.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('#') || line.starts_with('[') {
                continue;
            }

            if let Some((key, value)) = line.split_once('=') {
                let key = key.trim();
                let value = value.trim().trim_matches('"');

                match key {
                    "backend" => {
                        if let Ok(b) = value.parse() {
                            config.backend = b;
                        }
                    }
                    "fallback" => {
                        if let Ok(b) = value.parse() {
                            config.fallback = b;
                        }
                    }
                    "debug" => {
                        config.debug = value == "true" || value == "1";
                    }
                    "vrr" => {
                        config.display.vrr = value == "true" || value == "1";
                    }
                    "hdr" => {
                        config.display.hdr = value == "true" || value == "1";
                    }
                    "refresh_rate" => {
                        if let Ok(rate) = value.parse() {
                            config.display.refresh_rate = Some(rate);
                        }
                    }
                    "resolution" => {
                        if let Some((w, h)) = value.split_once('x') {
                            if let (Ok(w), Ok(h)) = (w.parse(), h.parse()) {
                                config.display.resolution = Some((w, h));
                            }
                        }
                    }
                    "xr_enabled" => {
                        config.xr.enabled = value == "true" || value == "1";
                    }
                    "xr_mode" => {
                        config.xr.mode = value.to_string();
                    }
                    "supersampling" => {
                        if let Ok(ss) = value.parse() {
                            config.xr.supersampling = ss;
                        }
                    }
                    _ => {}
                }
            }
        }

        Ok(config)
    }

    /// Detect the best backend based on available hardware
    fn detect_best_backend() -> Backend {
        // Check for XR hardware first
        if Self::has_xr_hardware() {
            return Backend::Xr;
        }

        // Check for display server (indicates we're not on bare metal)
        #[cfg(target_os = "linux")]
        {
            let has_display = std::env::var("DISPLAY").is_ok()
                || std::env::var("WAYLAND_DISPLAY").is_ok();

            if has_display {
                // Running under existing display server - use winit
                return Backend::Winit;
            }

            // Check if we have DRM access
            if Self::has_drm_access() {
                return Backend::Smithay;
            }
        }

        // Windows/macOS - use winit
        #[cfg(any(target_os = "windows", target_os = "macos"))]
        {
            return Backend::Winit;
        }

        // Fallback to CLI
        Backend::Cli
    }

    /// Check if XR hardware is available
    fn has_xr_hardware() -> bool {
        // Check for OpenXR runtime
        #[cfg(target_os = "linux")]
        {
            // Check for common OpenXR runtime paths
            let runtime_paths: [&str; 2] = [
                "/usr/share/openxr/1/openxr_runtime.json",
                "/etc/xdg/openxr/1/openxr_runtime.json",
            ];

            for path in &runtime_paths {
                if !path.is_empty() && Path::new(path).exists() {
                    log::debug!("Found OpenXR runtime at: {}", path);
                    return true;
                }
            }

            // Check environment variable separately
            if let Ok(env_path) = std::env::var("XR_RUNTIME_JSON") {
                if !env_path.is_empty() && Path::new(&env_path).exists() {
                    log::debug!("Found OpenXR runtime at: {}", env_path);
                    return true;
                }
            }
        }

        #[cfg(target_os = "windows")]
        {
            // Check Windows registry for OpenXR runtime
            // This is a simplified check
            if std::env::var("XR_RUNTIME_JSON").is_ok() {
                return true;
            }
        }

        false
    }

    /// Check if we have DRM access (can render directly to display)
    #[cfg(target_os = "linux")]
    fn has_drm_access() -> bool {
        use std::os::unix::fs::MetadataExt;

        // Check for DRM device
        let drm_paths = ["/dev/dri/card0", "/dev/dri/card1"];

        for path in &drm_paths {
            if let Ok(metadata) = std::fs::metadata(path) {
                // Check if we can access the device
                // (would need to be in video group or root)
                log::debug!("Found DRM device: {} (mode: {:o})", path, metadata.mode());
                return true;
            }
        }

        false
    }

    /// Get the effective backend (after auto-detection)
    pub fn effective_backend(&self) -> Backend {
        if self.backend == Backend::Auto {
            Self::detect_best_backend()
        } else {
            self.backend
        }
    }

    /// Check if a specific backend is available
    pub fn is_backend_available(&self, backend: Backend) -> bool {
        match backend {
            Backend::Smithay => {
                #[cfg(all(feature = "smithay", target_os = "linux"))]
                {
                    return Self::has_drm_access();
                }
                #[cfg(not(all(feature = "smithay", target_os = "linux")))]
                {
                    return false;
                }
            }
            Backend::Winit => {
                #[cfg(any(target_os = "linux", target_os = "windows", target_os = "macos"))]
                {
                    #[cfg(target_os = "linux")]
                    {
                        return std::env::var("DISPLAY").is_ok()
                            || std::env::var("WAYLAND_DISPLAY").is_ok();
                    }
                    #[cfg(not(target_os = "linux"))]
                    {
                        return true;
                    }
                }
                #[cfg(not(any(target_os = "linux", target_os = "windows", target_os = "macos")))]
                {
                    return false;
                }
            }
            Backend::Xr => Self::has_xr_hardware(),
            Backend::Cli => true, // Always available
            Backend::Auto => true,
        }
    }

    /// Print configuration summary
    pub fn print_summary(&self) {
        log::info!("Boot Configuration:");
        log::info!("  Backend: {} (effective: {})", self.backend, self.effective_backend());
        log::info!("  Fallback: {}", self.fallback);
        log::info!("  VRR: {}, HDR: {}", self.display.vrr, self.display.hdr);
        if self.xr.enabled {
            log::info!("  XR: enabled, mode={}, SS={}", self.xr.mode, self.xr.supersampling);
        }
        if let Some(path) = &self.config_path {
            log::info!("  Config: {}", path);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_backend_parse() {
        assert_eq!("smithay".parse::<Backend>().unwrap(), Backend::Smithay);
        assert_eq!("winit".parse::<Backend>().unwrap(), Backend::Winit);
        assert_eq!("xr".parse::<Backend>().unwrap(), Backend::Xr);
        assert_eq!("cli".parse::<Backend>().unwrap(), Backend::Cli);
        assert_eq!("auto".parse::<Backend>().unwrap(), Backend::Auto);
    }

    #[test]
    fn test_backend_display() {
        assert_eq!(Backend::Smithay.to_string(), "smithay");
        assert_eq!(Backend::Winit.to_string(), "winit");
    }

    #[test]
    fn test_default_config() {
        let config = BootConfig::default();
        assert_eq!(config.backend, Backend::Auto);
        assert_eq!(config.fallback, Backend::Cli);
        assert!(config.display.vrr);
        assert!(!config.display.hdr);
    }

    #[test]
    fn test_cli_always_available() {
        let config = BootConfig::default();
        assert!(config.is_backend_available(Backend::Cli));
    }
}
