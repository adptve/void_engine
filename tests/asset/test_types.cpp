/// @file test_types.cpp
/// @brief Tests for void_asset types

#include <catch2/catch_test_macros.hpp>
#include <void_engine/asset/types.hpp>
#include <string>

using namespace void_asset;

// =============================================================================
// LoadState Tests
// =============================================================================

TEST_CASE("LoadState: names are correct", "[asset][types]") {
    REQUIRE(std::string(load_state_name(LoadState::NotLoaded)) == "NotLoaded");
    REQUIRE(std::string(load_state_name(LoadState::Loading)) == "Loading");
    REQUIRE(std::string(load_state_name(LoadState::Loaded)) == "Loaded");
    REQUIRE(std::string(load_state_name(LoadState::Failed)) == "Failed");
    REQUIRE(std::string(load_state_name(LoadState::Reloading)) == "Reloading");
}

// =============================================================================
// AssetId Tests
// =============================================================================

TEST_CASE("AssetId: default is invalid", "[asset][types]") {
    AssetId id;
    REQUIRE_FALSE(id.is_valid());
    REQUIRE(id.raw() == 0);
}

TEST_CASE("AssetId: explicit construction is valid", "[asset][types]") {
    AssetId id{42};
    REQUIRE(id.is_valid());
    REQUIRE(id.raw() == 42);
}

TEST_CASE("AssetId: invalid factory creates invalid id", "[asset][types]") {
    AssetId id = AssetId::invalid();
    REQUIRE_FALSE(id.is_valid());
    REQUIRE(id == AssetId{});
}

TEST_CASE("AssetId: comparison", "[asset][types]") {
    AssetId a{1};
    AssetId b{2};
    AssetId c{1};

    REQUIRE(a == c);
    REQUIRE(a != b);
    REQUIRE(a < b);
}

TEST_CASE("AssetId: hash works", "[asset][types]") {
    AssetId id{42};
    std::hash<AssetId> hasher;
    REQUIRE(hasher(id) == std::hash<std::uint64_t>{}(42));
}

// =============================================================================
// AssetPath Tests
// =============================================================================

TEST_CASE("AssetPath: default construction", "[asset][types]") {
    AssetPath path;
    REQUIRE(path.str().empty());
    REQUIRE(path.hash == 0);
}

TEST_CASE("AssetPath: construction from string", "[asset][types]") {
    AssetPath path("textures/player.png");
    REQUIRE(path.str() == "textures/player.png");
    REQUIRE(path.hash != 0);
}

TEST_CASE("AssetPath: normalizes backslashes", "[asset][types]") {
    AssetPath path("textures\\player.png");
    REQUIRE(path.str() == "textures/player.png");
}

TEST_CASE("AssetPath: removes trailing slashes", "[asset][types]") {
    AssetPath path("textures/sprites/");
    REQUIRE(path.str() == "textures/sprites");
}

TEST_CASE("AssetPath: extension extraction", "[asset][types]") {
    REQUIRE(AssetPath("file.txt").extension() == "txt");
    REQUIRE(AssetPath("path/to/file.png").extension() == "png");
    REQUIRE(AssetPath("noext").extension().empty());
    REQUIRE(AssetPath("multiple.dots.txt").extension() == "txt");
}

TEST_CASE("AssetPath: filename extraction", "[asset][types]") {
    REQUIRE(AssetPath("textures/player.png").filename() == "player.png");
    REQUIRE(AssetPath("file.txt").filename() == "file.txt");
    REQUIRE(AssetPath("a/b/c/d.e").filename() == "d.e");
}

TEST_CASE("AssetPath: directory extraction", "[asset][types]") {
    REQUIRE(AssetPath("textures/player.png").directory() == "textures");
    REQUIRE(AssetPath("file.txt").directory().empty());
    REQUIRE(AssetPath("a/b/c/d.e").directory() == "a/b/c");
}

TEST_CASE("AssetPath: stem extraction", "[asset][types]") {
    REQUIRE(AssetPath("textures/player.png").stem() == "player");
    REQUIRE(AssetPath("noext").stem() == "noext");
    REQUIRE(AssetPath("file.tar.gz").stem() == "file.tar");
}

TEST_CASE("AssetPath: comparison uses hash and string", "[asset][types]") {
    AssetPath a("file.txt");
    AssetPath b("file.txt");
    AssetPath c("other.txt");

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
}

TEST_CASE("AssetPath: hash works", "[asset][types]") {
    AssetPath path("test.txt");
    std::hash<AssetPath> hasher;
    REQUIRE(hasher(path) == static_cast<std::size_t>(path.hash));
}

// =============================================================================
// AssetTypeId Tests
// =============================================================================

TEST_CASE("AssetTypeId: of() creates correct type id", "[asset][types]") {
    auto type_id = AssetTypeId::of<int>();
    REQUIRE(type_id.type_id == std::type_index(typeid(int)));
}

TEST_CASE("AssetTypeId: comparison", "[asset][types]") {
    auto a = AssetTypeId::of<int>();
    auto b = AssetTypeId::of<int>();
    auto c = AssetTypeId::of<float>();

    REQUIRE(a == b);
    REQUIRE_FALSE(a == c);
}

// =============================================================================
// AssetMetadata Tests
// =============================================================================

TEST_CASE("AssetMetadata: default state", "[asset][types]") {
    AssetMetadata meta;
    REQUIRE(meta.state == LoadState::NotLoaded);
    REQUIRE(meta.generation == 0);
    REQUIRE(meta.size_bytes == 0);
    REQUIRE_FALSE(meta.is_loaded());
    REQUIRE_FALSE(meta.is_loading());
    REQUIRE_FALSE(meta.is_failed());
}

TEST_CASE("AssetMetadata: mark_loading", "[asset][types]") {
    AssetMetadata meta;
    meta.mark_loading();
    REQUIRE(meta.state == LoadState::Loading);
    REQUIRE(meta.is_loading());
}

TEST_CASE("AssetMetadata: mark_loaded", "[asset][types]") {
    AssetMetadata meta;
    meta.mark_loaded(1024);
    REQUIRE(meta.state == LoadState::Loaded);
    REQUIRE(meta.is_loaded());
    REQUIRE(meta.size_bytes == 1024);
    REQUIRE(meta.generation == 1);
}

TEST_CASE("AssetMetadata: mark_failed", "[asset][types]") {
    AssetMetadata meta;
    meta.mark_failed("File not found");
    REQUIRE(meta.state == LoadState::Failed);
    REQUIRE(meta.is_failed());
    REQUIRE(meta.error_message == "File not found");
}

TEST_CASE("AssetMetadata: mark_reloading", "[asset][types]") {
    AssetMetadata meta;
    meta.mark_loaded();
    meta.mark_reloading();
    REQUIRE(meta.state == LoadState::Reloading);
    REQUIRE(meta.is_loading());
}

TEST_CASE("AssetMetadata: dependencies", "[asset][types]") {
    AssetMetadata meta;
    meta.add_dependency(AssetId{1});
    meta.add_dependency(AssetId{2});
    REQUIRE(meta.dependencies.size() == 2);
    REQUIRE(meta.dependencies[0] == AssetId{1});
    REQUIRE(meta.dependencies[1] == AssetId{2});
}

TEST_CASE("AssetMetadata: dependents", "[asset][types]") {
    AssetMetadata meta;
    meta.add_dependent(AssetId{3});
    REQUIRE(meta.dependents.size() == 1);
    REQUIRE(meta.dependents[0] == AssetId{3});
}

// =============================================================================
// AssetEvent Tests
// =============================================================================

TEST_CASE("AssetEvent: factory methods", "[asset][types]") {
    AssetId id{42};
    AssetPath path("test.txt");

    auto loaded = AssetEvent::loaded(id, path);
    REQUIRE(loaded.type == AssetEventType::Loaded);
    REQUIRE(loaded.id == id);
    REQUIRE(loaded.path == path);

    auto failed = AssetEvent::failed(id, path, "error");
    REQUIRE(failed.type == AssetEventType::Failed);
    REQUIRE(failed.error == "error");

    auto reloaded = AssetEvent::reloaded(id, path, 5);
    REQUIRE(reloaded.type == AssetEventType::Reloaded);
    REQUIRE(reloaded.generation == 5);

    auto unloaded = AssetEvent::unloaded(id, path);
    REQUIRE(unloaded.type == AssetEventType::Unloaded);

    auto changed = AssetEvent::file_changed(path);
    REQUIRE(changed.type == AssetEventType::FileChanged);
    REQUIRE_FALSE(changed.id.is_valid());
}

TEST_CASE("AssetEvent: type names", "[asset][types]") {
    REQUIRE(std::string(asset_event_type_name(AssetEventType::Loaded)) == "Loaded");
    REQUIRE(std::string(asset_event_type_name(AssetEventType::Failed)) == "Failed");
    REQUIRE(std::string(asset_event_type_name(AssetEventType::Reloaded)) == "Reloaded");
    REQUIRE(std::string(asset_event_type_name(AssetEventType::Unloaded)) == "Unloaded");
    REQUIRE(std::string(asset_event_type_name(AssetEventType::FileChanged)) == "FileChanged");
}

// =============================================================================
// AssetError Tests
// =============================================================================

TEST_CASE("AssetError: not_found", "[asset][types]") {
    auto error = AssetError::not_found("missing.txt");
    REQUIRE(error.code() == void_core::ErrorCode::NotFound);
    REQUIRE(error.message().find("missing.txt") != std::string::npos);
}

TEST_CASE("AssetError: already_loaded", "[asset][types]") {
    auto error = AssetError::already_loaded("loaded.txt");
    REQUIRE(error.code() == void_core::ErrorCode::AlreadyExists);
}

TEST_CASE("AssetError: load_failed", "[asset][types]") {
    auto error = AssetError::load_failed("file.txt", "IO error");
    REQUIRE(error.code() == void_core::ErrorCode::IOError);
    REQUIRE(error.message().find("file.txt") != std::string::npos);
    REQUIRE(error.message().find("IO error") != std::string::npos);
}

TEST_CASE("AssetError: no_loader", "[asset][types]") {
    auto error = AssetError::no_loader("xyz");
    REQUIRE(error.code() == void_core::ErrorCode::NotFound);
    REQUIRE(error.message().find("xyz") != std::string::npos);
}

TEST_CASE("AssetError: parse_error", "[asset][types]") {
    auto error = AssetError::parse_error("data.json", "Invalid JSON");
    REQUIRE(error.code() == void_core::ErrorCode::ParseError);
}

TEST_CASE("AssetError: dependency_failed", "[asset][types]") {
    auto error = AssetError::dependency_failed("material.mat", "texture.png");
    REQUIRE(error.code() == void_core::ErrorCode::DependencyMissing);
}
