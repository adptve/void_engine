//! VRR and HDR Demo
//!
//! This example demonstrates VRR (Variable Refresh Rate) and HDR support
//! in the void_compositor.
//!
//! Run with:
//! ```
//! cargo run --example vrr_hdr_demo --features smithay-compositor
//! ```

use void_compositor::{
    Compositor, CompositorConfig, VrrMode, HdrConfig, TransferFunction,
};

fn main() {
    env_logger::init();

    println!("VRR and HDR Demo");
    println!("================\n");

    // Create compositor
    let config = CompositorConfig::default();
    let mut compositor = match Compositor::new(config) {
        Ok(c) => c,
        Err(e) => {
            eprintln!("Failed to create compositor: {}", e);
            eprintln!("Note: This example requires Linux with DRM/KMS access");
            return;
        }
    };

    // Query capabilities
    let caps = compositor.capabilities();
    println!("Compositor Capabilities:");
    println!("  Displays: {}", caps.display_count);
    println!("  Resolution: {}x{}", caps.current_resolution.0, caps.current_resolution.1);
    println!("  Refresh rates: {:?}", caps.refresh_rates);
    println!("  VRR supported: {}", caps.vrr_supported);
    println!("  HDR supported: {}", caps.hdr_supported);
    println!();

    // Check VRR capability
    if let Some(vrr_cap) = compositor.vrr_capability() {
        if vrr_cap.supported {
            println!("VRR Capability:");
            println!("  Range: {}-{} Hz",
                vrr_cap.min_refresh_rate.unwrap_or(0),
                vrr_cap.max_refresh_rate.unwrap_or(0)
            );
            if let Some(tech) = &vrr_cap.technology {
                println!("  Technology: {}", tech);
            }
            println!();

            // Enable VRR in Auto mode
            match compositor.enable_vrr(VrrMode::Auto) {
                Ok(_) => println!("VRR enabled in Auto mode"),
                Err(e) => eprintln!("Failed to enable VRR: {}", e),
            }

            // Check active VRR config
            if let Some(vrr_config) = compositor.vrr_config() {
                println!("  Active VRR config: {}", vrr_config.range_string());
                println!("  Current refresh: {} Hz", vrr_config.current_refresh_rate);
            }
            println!();
        } else {
            println!("VRR not supported on this display\n");
        }
    }

    // Check HDR capability
    if let Some(hdr_cap) = compositor.hdr_capability() {
        if hdr_cap.supported {
            println!("HDR Capability:");
            println!("  Transfer functions: {:?}",
                hdr_cap.transfer_functions.iter()
                    .map(|tf| tf.name())
                    .collect::<Vec<_>>()
            );
            if let Some(max_nits) = hdr_cap.max_luminance {
                println!("  Max luminance: {} nits", max_nits);
            }
            if let Some(min_nits) = hdr_cap.min_luminance {
                println!("  Min luminance: {:.4} nits", min_nits);
            }
            println!("  Color gamuts: {:?}",
                hdr_cap.color_gamuts.iter()
                    .map(|cg| cg.name())
                    .collect::<Vec<_>>()
            );
            println!();

            // Enable HDR10 with 1000 nits
            let hdr_config = HdrConfig::hdr10(1000);
            match compositor.enable_hdr(hdr_config) {
                Ok(_) => println!("HDR enabled with HDR10 (1000 nits)"),
                Err(e) => eprintln!("Failed to enable HDR: {}", e),
            }

            // Check active HDR config
            if let Some(hdr_cfg) = compositor.hdr_config() {
                println!("  Active HDR config:");
                println!("    Transfer: {}", hdr_cfg.transfer_function.name());
                println!("    Color space: {}", hdr_cfg.color_primaries.name());
                println!("    Max luminance: {} nits", hdr_cfg.max_luminance);
            }
            println!();
        } else {
            println!("HDR not supported on this display\n");
        }
    }

    // Run for a few frames to demonstrate
    println!("Running compositor for demonstration...");
    println!("(Press Ctrl+C to exit)\n");

    let mut frame_count = 0;
    while compositor.is_running() && frame_count < 100 {
        // Dispatch events
        if let Err(e) = compositor.dispatch() {
            eprintln!("Dispatch error: {}", e);
            break;
        }

        // Check if we should render
        if compositor.should_render() {
            // Begin frame
            if let Ok(target) = compositor.begin_frame() {
                frame_count += 1;

                // Simulate content velocity for VRR adaptation
                // In a real application, this would be based on scene changes
                let velocity = if frame_count % 60 < 30 {
                    0.8 // Fast-moving content
                } else {
                    0.1 // Static content
                };

                compositor.frame_scheduler_mut().update_content_velocity(velocity);

                // Log VRR adaptation every 10 frames
                if frame_count % 10 == 0 {
                    if let Some(vrr) = compositor.vrr_config() {
                        if vrr.is_active() {
                            println!("Frame {}: VRR refresh = {} Hz, velocity = {:.2}",
                                frame_count,
                                vrr.current_refresh_rate,
                                compositor.frame_scheduler().content_velocity()
                            );
                        }
                    }
                }

                // End frame
                if let Err(e) = compositor.end_frame(target) {
                    eprintln!("End frame error: {}", e);
                }
            }
        }

        // Limit to prevent busy loop
        std::thread::sleep(std::time::Duration::from_millis(1));
    }

    println!("\nDemo complete!");

    // Disable VRR and HDR before shutdown
    if compositor.vrr_config().is_some() {
        let _ = compositor.disable_vrr();
        println!("VRR disabled");
    }

    if compositor.hdr_config().is_some() {
        let _ = compositor.disable_hdr();
        println!("HDR disabled");
    }

    compositor.shutdown();
}
