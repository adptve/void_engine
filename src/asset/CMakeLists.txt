# void_asset - Asset management
void_add_module(NAME void_asset
    SOURCES
        # Core asset system
        asset.cpp
        cache.cpp
        handle.cpp
        loader.cpp
        storage.cpp
        server.cpp
        # Network and remote assets
        remote.cpp
        http_client.cpp
        websocket_client.cpp
        # Asset loaders
        loaders/texture_loader.cpp
        loaders/model_loader.cpp
        loaders/shader_loader.cpp
        loaders/audio_loader.cpp
        # Third-party implementations (header-only libraries)
        stb_impl.cpp
    DEPENDENCIES
        void_core
        void_structures
        nlohmann_json::nlohmann_json
)

# Optional: libcurl for HTTP client
find_package(CURL QUIET)
if(CURL_FOUND)
    target_link_libraries(void_asset PRIVATE CURL::libcurl)
    target_compile_definitions(void_asset PRIVATE VOID_HAS_CURL)
    message(STATUS "void_asset: libcurl enabled for HTTP client")
endif()

# Optional: Boost.Beast for WebSocket client
find_package(Boost COMPONENTS system QUIET)
find_package(OpenSSL QUIET)
if(Boost_FOUND AND OpenSSL_FOUND)
    target_link_libraries(void_asset PRIVATE
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
    )
    target_compile_definitions(void_asset PRIVATE VOID_HAS_BEAST)
    message(STATUS "void_asset: Boost.Beast enabled for WebSocket client")
endif()

# stb - Image loading (texture_loader)
if(TARGET stb)
    target_link_libraries(void_asset PRIVATE stb)
    target_compile_definitions(void_asset PRIVATE VOID_HAS_STB)
    message(STATUS "void_asset: stb enabled for texture loading")
elseif(DEFINED STB_SOURCE_DIR)
    target_include_directories(void_asset PRIVATE ${STB_SOURCE_DIR})
    target_compile_definitions(void_asset PRIVATE VOID_HAS_STB)
    message(STATUS "void_asset: stb enabled for texture loading (include dir)")
endif()

# tinygltf - glTF model loading (model_loader)
if(TARGET tinygltf)
    target_link_libraries(void_asset PRIVATE tinygltf)
    target_compile_definitions(void_asset PRIVATE VOID_HAS_TINYGLTF)
    message(STATUS "void_asset: tinygltf enabled for model loading")
elseif(DEFINED TINYGLTF_SOURCE_DIR)
    target_include_directories(void_asset PRIVATE ${TINYGLTF_SOURCE_DIR})
    target_compile_definitions(void_asset PRIVATE VOID_HAS_TINYGLTF)
    message(STATUS "void_asset: tinygltf enabled for model loading (include dir)")
endif()

# dr_libs - Audio decoding (dr_wav, dr_flac, dr_mp3)
if(TARGET dr_libs)
    target_link_libraries(void_asset PRIVATE dr_libs)
    target_compile_definitions(void_asset PRIVATE VOID_HAS_DR_LIBS)
    message(STATUS "void_asset: dr_libs enabled for audio decoding")
elseif(DEFINED DR_LIBS_SOURCE_DIR)
    target_include_directories(void_asset PRIVATE ${DR_LIBS_SOURCE_DIR})
    target_compile_definitions(void_asset PRIVATE VOID_HAS_DR_LIBS)
    message(STATUS "void_asset: dr_libs enabled for audio decoding (include dir)")
endif()

# minimp3 - MP3 decoding
if(TARGET minimp3)
    target_link_libraries(void_asset PRIVATE minimp3)
    target_compile_definitions(void_asset PRIVATE VOID_HAS_MINIMP3)
    message(STATUS "void_asset: minimp3 enabled for MP3 decoding")
elseif(DEFINED MINIMP3_SOURCE_DIR)
    target_include_directories(void_asset PRIVATE ${MINIMP3_SOURCE_DIR})
    target_compile_definitions(void_asset PRIVATE VOID_HAS_MINIMP3)
    message(STATUS "void_asset: minimp3 enabled for MP3 decoding (include dir)")
endif()
