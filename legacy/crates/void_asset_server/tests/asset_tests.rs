//! Integration tests for void_asset_server

use void_asset_server::*;
use std::path::PathBuf;
use std::fs;

#[test]
fn test_asset_kind_detection() {
    assert_eq!(AssetKind::from_path(&PathBuf::from("shader.wgsl")), AssetKind::Shader);
    assert_eq!(AssetKind::from_path(&PathBuf::from("texture.png")), AssetKind::Texture);
    assert_eq!(AssetKind::from_path(&PathBuf::from("model.obj")), AssetKind::Mesh);
    assert_eq!(AssetKind::from_path(&PathBuf::from("scene.json")), AssetKind::Scene);
    assert_eq!(AssetKind::from_path(&PathBuf::from("unknown.xyz")), AssetKind::Unknown);
}

#[test]
fn test_injector_creation() {
    let config = InjectorConfig {
        watch_dirs: vec![PathBuf::from("test_assets")],
        hot_reload: false, // Disable for testing
        asset_dir: PathBuf::from("test_assets"),
    };

    let injector = AssetInjector::new(config);
    assert_eq!(injector.list_assets().len(), 0);
}

#[test]
fn test_shader_loading_from_bytes() {
    let config = InjectorConfig {
        watch_dirs: vec![],
        hot_reload: false,
        asset_dir: PathBuf::from("."),
    };

    let injector = AssetInjector::new(config);

    let shader_source = r#"
        @vertex
        fn vs_main(@builtin(vertex_index) idx: u32) -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 1.0);
        }

        @fragment
        fn fs_main() -> @location(0) vec4<f32> {
            return vec4<f32>(1.0, 0.0, 0.0, 1.0);
        }
    "#;

    let id = injector.load_bytes("test.wgsl", shader_source.as_bytes())
        .expect("Failed to load shader");

    assert!(id > 0);

    // Verify we can retrieve it
    let shader = injector.get_shader(id).expect("Shader not found");
    assert!(shader.source.contains("vs_main"));
    assert!(shader.source.contains("fs_main"));
}

#[test]
fn test_asset_reload_increments_generation() {
    let config = InjectorConfig {
        watch_dirs: vec![],
        hot_reload: false,
        asset_dir: PathBuf::from("."),
    };

    let injector = AssetInjector::new(config);

    let shader_v1 = "fn main() { }";
    let id1 = injector.load_bytes("shader.wgsl", shader_v1.as_bytes()).unwrap();
    let gen1 = injector.get_generation(id1).unwrap();

    // Reload same path
    let shader_v2 = "fn main() { /* updated */ }";
    let id2 = injector.load_bytes("shader.wgsl", shader_v2.as_bytes()).unwrap();
    let gen2 = injector.get_generation(id2).unwrap();

    // Same ID, incremented generation
    assert_eq!(id1, id2);
    assert_eq!(gen2, gen1 + 1);
}

#[test]
fn test_asset_by_path_lookup() {
    let config = InjectorConfig {
        watch_dirs: vec![],
        hot_reload: false,
        asset_dir: PathBuf::from("."),
    };

    let injector = AssetInjector::new(config);

    let id = injector.load_bytes("my_shader.wgsl", b"fn main() {}").unwrap();

    // Lookup by path
    let found_id = injector.get_by_path("my_shader.wgsl").expect("Not found");
    assert_eq!(found_id, id);
}

#[test]
fn test_injection_events() {
    let config = InjectorConfig {
        watch_dirs: vec![],
        hot_reload: false,
        asset_dir: PathBuf::from("."),
    };

    let mut injector = AssetInjector::new(config);

    // Load asset
    injector.load_bytes("test.wgsl", b"fn main() {}").unwrap();

    // Check events (would be drained in real usage)
    let events = injector.update();
    assert_eq!(events.len(), 1);

    if let InjectionEvent::Loaded { path, kind, .. } = &events[0] {
        assert_eq!(path, "test.wgsl");
        assert_eq!(*kind, AssetKind::Shader);
    } else {
        panic!("Expected Loaded event");
    }
}

#[test]
fn test_multiple_asset_types() {
    let config = InjectorConfig {
        watch_dirs: vec![],
        hot_reload: false,
        asset_dir: PathBuf::from("."),
    };

    let injector = AssetInjector::new(config);

    // Load different asset types
    let shader_id = injector.load_bytes("shader.wgsl", b"fn main() {}").unwrap();
    let scene_id = injector.load_bytes("scene.json", b"{\"entities\": []}").unwrap();

    // Verify correct types
    assert!(injector.get_shader(shader_id).is_some());
    assert!(injector.get_shader(scene_id).is_none()); // Wrong type

    assert!(injector.get_scene(scene_id).is_some());
    assert!(injector.get_scene(shader_id).is_none()); // Wrong type
}

#[test]
fn test_list_assets() {
    let config = InjectorConfig {
        watch_dirs: vec![],
        hot_reload: false,
        asset_dir: PathBuf::from("."),
    };

    let injector = AssetInjector::new(config);

    injector.load_bytes("a.wgsl", b"").unwrap();
    injector.load_bytes("b.wgsl", b"").unwrap();
    injector.load_bytes("c.json", b"{}").unwrap();

    let assets = injector.list_assets();
    assert_eq!(assets.len(), 3);

    // Check we have the right types
    let shader_count = assets.iter().filter(|(_, _, k)| *k == AssetKind::Shader).count();
    let scene_count = assets.iter().filter(|(_, _, k)| *k == AssetKind::Scene).count();

    assert_eq!(shader_count, 2);
    assert_eq!(scene_count, 1);
}

#[test]
fn test_file_watcher_creation() {
    let watcher = FileWatcher::new();
    assert!(watcher.is_ok());
}

#[test]
fn test_file_watcher_watch_dirs() {
    let mut watcher = FileWatcher::new().unwrap();
    let test_dir = std::env::temp_dir().join("void_test_watch");

    // Create directory if it doesn't exist
    let _ = std::fs::create_dir_all(&test_dir);

    let result = watcher.watch(&test_dir);
    assert!(result.is_ok());

    assert_eq!(watcher.watch_dirs().len(), 1);
    assert_eq!(watcher.watch_dirs()[0], test_dir);

    // Cleanup
    let _ = std::fs::remove_dir_all(&test_dir);
}

#[test]
fn test_file_change_debouncing() {
    let mut watcher = FileWatcher::new().unwrap();
    watcher.set_debounce(std::time::Duration::from_millis(50));

    // Poll should return empty initially
    let changes = watcher.poll();
    assert_eq!(changes.len(), 0);
}

#[test]
fn test_scene_loader() {
    use void_asset_server::loaders::SceneLoader;

    let scene_json = r#"{
        "entities": [
            {
                "id": 1,
                "name": "Player",
                "components": {
                    "Transform": {
                        "position": [0.0, 0.0, 0.0],
                        "rotation": [0.0, 0.0, 0.0, 1.0],
                        "scale": [1.0, 1.0, 1.0]
                    }
                }
            }
        ]
    }"#;

    let scene = SceneLoader::load(scene_json.as_bytes(), "test.json")
        .expect("Failed to load scene");

    assert_eq!(scene.entities.len(), 1);
    assert_eq!(scene.entities[0].id, 1);
    assert_eq!(scene.entities[0].name.as_deref(), Some("Player"));
}

#[test]
fn test_shader_loader() {
    use void_asset_server::loaders::ShaderLoader;

    let source = r#"
        @vertex
        fn main() -> @builtin(position) vec4<f32> {
            return vec4<f32>(0.0);
        }
    "#;

    let shader = ShaderLoader::load(source.as_bytes(), "test.wgsl")
        .expect("Failed to load shader");

    assert!(shader.source.contains("@vertex"));
    assert!(shader.source.contains("main"));
    assert_eq!(shader.path, "test.wgsl");
}

#[test]
fn test_asset_ref() {
    let config = InjectorConfig::default();
    let injector = AssetInjector::new(config);

    let id = injector.load_bytes("test.wgsl", b"fn main() {}").unwrap();

    // Get asset ref
    let asset_ref = injector.get(id).expect("Asset not found");
    assert_eq!(asset_ref.kind(), Some(AssetKind::Shader));
    assert_eq!(asset_ref.path(), Some("test.wgsl"));
    assert_eq!(asset_ref.generation(), Some(1));
}

#[test]
fn test_injector_start_without_hot_reload() {
    let config = InjectorConfig {
        watch_dirs: vec![],
        hot_reload: false,
        asset_dir: PathBuf::from("."),
    };

    let mut injector = AssetInjector::new(config);
    let result = injector.start();
    assert!(result.is_ok());
}

#[test]
fn test_failed_asset_loading() {
    let config = InjectorConfig::default();
    let injector = AssetInjector::new(config);

    // Try to load invalid JSON as scene
    let result = injector.load_bytes("invalid.json", b"this is not json");
    assert!(result.is_err());
}
