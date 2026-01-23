/// @file test_loader.cpp
/// @brief Tests for void_asset loader system

#include <catch2/catch_test_macros.hpp>
#include <void_engine/asset/loader.hpp>
#include <string>
#include <vector>

using namespace void_asset;

// =============================================================================
// LoadContext Tests
// =============================================================================

TEST_CASE("LoadContext: basic access", "[asset][loader]") {
    std::vector<std::uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};  // "Hello"
    AssetPath path("test/file.txt");
    AssetId id{42};

    LoadContext ctx(data, path, id);

    REQUIRE(ctx.data().size() == 5);
    REQUIRE(ctx.path().str() == "test/file.txt");
    REQUIRE(ctx.id() == AssetId{42});
    REQUIRE(ctx.extension() == "txt");
    REQUIRE(ctx.size() == 5);
}

TEST_CASE("LoadContext: data_as_string", "[asset][loader]") {
    std::vector<std::uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
    AssetPath path("file.txt");
    LoadContext ctx(data, path, AssetId{1});

    REQUIRE(ctx.data_as_string() == "Hello");
}

TEST_CASE("LoadContext: dependencies", "[asset][loader]") {
    std::vector<std::uint8_t> data;
    AssetPath path("main.txt");
    LoadContext ctx(data, path, AssetId{1});

    ctx.add_dependency(AssetPath("dep1.txt"));
    ctx.add_dependency(AssetPath("dep2.txt"));

    REQUIRE(ctx.dependencies().size() == 2);
    REQUIRE(ctx.dependencies()[0].str() == "dep1.txt");
    REQUIRE(ctx.dependencies()[1].str() == "dep2.txt");
}

TEST_CASE("LoadContext: dependency IDs", "[asset][loader]") {
    std::vector<std::uint8_t> data;
    AssetPath path("main.txt");
    LoadContext ctx(data, path, AssetId{1});

    ctx.add_dependency(AssetId{10});
    ctx.add_dependency(AssetId{20});

    REQUIRE(ctx.dependency_ids().size() == 2);
    REQUIRE(ctx.dependency_ids()[0] == AssetId{10});
    REQUIRE(ctx.dependency_ids()[1] == AssetId{20});
}

TEST_CASE("LoadContext: metadata", "[asset][loader]") {
    std::vector<std::uint8_t> data;
    AssetPath path("file.txt");
    LoadContext ctx(data, path, AssetId{1});

    ctx.set_metadata("key1", "value1");
    ctx.set_metadata("key2", "value2");

    auto* val1 = ctx.get_metadata("key1");
    auto* val2 = ctx.get_metadata("key2");
    auto* missing = ctx.get_metadata("nonexistent");

    REQUIRE(val1 != nullptr);
    REQUIRE(*val1 == "value1");
    REQUIRE(val2 != nullptr);
    REQUIRE(*val2 == "value2");
    REQUIRE(missing == nullptr);
}

// =============================================================================
// BytesLoader Tests
// =============================================================================

TEST_CASE("BytesLoader: extensions", "[asset][loader]") {
    BytesLoader loader;
    auto exts = loader.extensions();
    REQUIRE(exts.size() == 2);
    REQUIRE(std::find(exts.begin(), exts.end(), "bin") != exts.end());
    REQUIRE(std::find(exts.begin(), exts.end(), "dat") != exts.end());
}

TEST_CASE("BytesLoader: type_id", "[asset][loader]") {
    BytesLoader loader;
    REQUIRE(loader.type_id() == std::type_index(typeid(BytesAsset)));
}

TEST_CASE("BytesLoader: type_name", "[asset][loader]") {
    BytesLoader loader;
    REQUIRE(loader.type_name() == "BytesAsset");
}

TEST_CASE("BytesLoader: load", "[asset][loader]") {
    BytesLoader loader;
    std::vector<std::uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    AssetPath path("test.bin");
    LoadContext ctx(data, path, AssetId{1});

    auto result = loader.load(ctx);
    REQUIRE(result);
    REQUIRE(result.value()->data == data);
}

// =============================================================================
// TextLoader Tests
// =============================================================================

TEST_CASE("TextLoader: extensions", "[asset][loader]") {
    TextLoader loader;
    auto exts = loader.extensions();
    REQUIRE(std::find(exts.begin(), exts.end(), "txt") != exts.end());
    REQUIRE(std::find(exts.begin(), exts.end(), "json") != exts.end());
    REQUIRE(std::find(exts.begin(), exts.end(), "md") != exts.end());
    REQUIRE(std::find(exts.begin(), exts.end(), "yaml") != exts.end());
}

TEST_CASE("TextLoader: type_id", "[asset][loader]") {
    TextLoader loader;
    REQUIRE(loader.type_id() == std::type_index(typeid(TextAsset)));
}

TEST_CASE("TextLoader: type_name", "[asset][loader]") {
    TextLoader loader;
    REQUIRE(loader.type_name() == "TextAsset");
}

TEST_CASE("TextLoader: load", "[asset][loader]") {
    TextLoader loader;
    std::string content = "Hello, World!";
    std::vector<std::uint8_t> data(content.begin(), content.end());
    AssetPath path("test.txt");
    LoadContext ctx(data, path, AssetId{1});

    auto result = loader.load(ctx);
    REQUIRE(result);
    REQUIRE(result.value()->text == "Hello, World!");
}

// =============================================================================
// TypedErasedLoader Tests
// =============================================================================

TEST_CASE("TypedErasedLoader: wraps typed loader", "[asset][loader]") {
    auto bytes_loader = std::make_unique<BytesLoader>();
    TypedErasedLoader<BytesAsset> erased(std::move(bytes_loader));

    REQUIRE(erased.type_id() == std::type_index(typeid(BytesAsset)));
    REQUIRE(erased.type_name() == "BytesAsset");
    REQUIRE(erased.extensions().size() == 2);
}

TEST_CASE("TypedErasedLoader: load_erased", "[asset][loader]") {
    auto bytes_loader = std::make_unique<BytesLoader>();
    TypedErasedLoader<BytesAsset> erased(std::move(bytes_loader));

    std::vector<std::uint8_t> data = {0xAA, 0xBB, 0xCC};
    AssetPath path("test.bin");
    LoadContext ctx(data, path, AssetId{1});

    auto result = erased.load_erased(ctx);
    REQUIRE(result);

    auto* asset = static_cast<BytesAsset*>(result.value());
    REQUIRE(asset->data == data);

    erased.delete_asset(asset);
}

// =============================================================================
// LoaderRegistry Tests
// =============================================================================

TEST_CASE("LoaderRegistry: default empty", "[asset][loader]") {
    LoaderRegistry registry;
    REQUIRE(registry.len() == 0);
    REQUIRE(registry.supported_extensions().empty());
}

TEST_CASE("LoaderRegistry: register typed loader", "[asset][loader]") {
    LoaderRegistry registry;
    registry.register_loader(std::make_unique<BytesLoader>());

    REQUIRE(registry.len() == 1);
    REQUIRE(registry.supports_extension("bin"));
    REQUIRE(registry.supports_extension("dat"));
    REQUIRE_FALSE(registry.supports_extension("xyz"));
}

TEST_CASE("LoaderRegistry: find_by_extension", "[asset][loader]") {
    LoaderRegistry registry;
    registry.register_loader(std::make_unique<BytesLoader>());
    registry.register_loader(std::make_unique<TextLoader>());

    auto bin_loaders = registry.find_by_extension("bin");
    REQUIRE(bin_loaders.size() == 1);

    auto txt_loaders = registry.find_by_extension("txt");
    REQUIRE(txt_loaders.size() == 1);

    auto xyz_loaders = registry.find_by_extension("xyz");
    REQUIRE(xyz_loaders.empty());
}

TEST_CASE("LoaderRegistry: find_first", "[asset][loader]") {
    LoaderRegistry registry;
    registry.register_loader(std::make_unique<BytesLoader>());

    auto* loader = registry.find_first("bin");
    REQUIRE(loader != nullptr);
    REQUIRE(loader->type_id() == std::type_index(typeid(BytesAsset)));

    auto* missing = registry.find_first("xyz");
    REQUIRE(missing == nullptr);
}

TEST_CASE("LoaderRegistry: find_by_type", "[asset][loader]") {
    LoaderRegistry registry;
    registry.register_loader(std::make_unique<BytesLoader>());
    registry.register_loader(std::make_unique<TextLoader>());

    auto bytes_loaders = registry.find_by_type(std::type_index(typeid(BytesAsset)));
    REQUIRE(bytes_loaders.size() == 1);

    auto text_loaders = registry.find_by_type(std::type_index(typeid(TextAsset)));
    REQUIRE(text_loaders.size() == 1);

    auto int_loaders = registry.find_by_type(std::type_index(typeid(int)));
    REQUIRE(int_loaders.empty());
}

TEST_CASE("LoaderRegistry: supports_type", "[asset][loader]") {
    LoaderRegistry registry;
    registry.register_loader(std::make_unique<BytesLoader>());

    REQUIRE(registry.supports_type(std::type_index(typeid(BytesAsset))));
    REQUIRE_FALSE(registry.supports_type(std::type_index(typeid(TextAsset))));
}

TEST_CASE("LoaderRegistry: supported_extensions", "[asset][loader]") {
    LoaderRegistry registry;
    registry.register_loader(std::make_unique<BytesLoader>());
    registry.register_loader(std::make_unique<TextLoader>());

    auto exts = registry.supported_extensions();
    REQUIRE(std::find(exts.begin(), exts.end(), "bin") != exts.end());
    REQUIRE(std::find(exts.begin(), exts.end(), "txt") != exts.end());
}

TEST_CASE("LoaderRegistry: clear", "[asset][loader]") {
    LoaderRegistry registry;
    registry.register_loader(std::make_unique<BytesLoader>());
    REQUIRE(registry.len() == 1);

    registry.clear();
    REQUIRE(registry.len() == 0);
    REQUIRE_FALSE(registry.supports_extension("bin"));
}

TEST_CASE("LoaderRegistry: multiple loaders for same extension", "[asset][loader]") {
    // Custom loader for same extension
    struct CustomBinAsset { int x; };
    class CustomBinLoader : public AssetLoader<CustomBinAsset> {
    public:
        std::vector<std::string> extensions() const override { return {"bin"}; }
        LoadResult<CustomBinAsset> load(LoadContext&) override {
            return void_core::Ok(std::make_unique<CustomBinAsset>());
        }
    };

    LoaderRegistry registry;
    registry.register_loader(std::make_unique<BytesLoader>());
    registry.register_loader(std::make_unique<CustomBinLoader>());

    auto bin_loaders = registry.find_by_extension("bin");
    REQUIRE(bin_loaders.size() == 2);
}
