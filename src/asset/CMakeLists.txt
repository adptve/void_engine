# void_asset - Asset management
void_add_module(NAME void_asset
    SOURCES
        stub.cpp
        remote.cpp
        http_client.cpp
        websocket_client.cpp
    DEPENDENCIES
        void_core
        void_structures
        tomlplusplus::tomlplusplus
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
