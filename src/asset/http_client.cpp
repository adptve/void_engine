/// @file http_client.cpp
/// @brief HTTP client implementation using libcurl

#include <void_engine/asset/remote.hpp>

#ifdef VOID_HAS_CURL
#include <curl/curl.h>
#endif

#include <cstring>
#include <algorithm>

namespace void_asset {

#ifdef VOID_HAS_CURL

// =============================================================================
// libcurl HTTP Client Implementation
// =============================================================================

class CurlHttpClient : public IHttpClient {
public:
    explicit CurlHttpClient(const RemoteConfig& config)
        : m_config(config)
        , m_curl(nullptr)
    {
        // Initialize libcurl globally (should be done once per process)
        static bool curl_initialized = false;
        if (!curl_initialized) {
            curl_global_init(CURL_GLOBAL_ALL);
            curl_initialized = true;
        }

        m_curl = curl_easy_init();
        if (m_curl) {
            configure_curl();
        }
    }

    ~CurlHttpClient() override {
        if (m_curl) {
            curl_easy_cleanup(m_curl);
        }
    }

    // Non-copyable
    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;

    HttpResponse get(
        const std::string& url,
        const std::unordered_map<std::string, std::string>& headers) override {

        if (!m_curl) {
            return make_error_response(500, "CURL not initialized");
        }

        std::lock_guard lock(m_mutex);

        // Reset curl handle
        curl_easy_reset(m_curl);
        configure_curl();

        // Set URL
        curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());

        // Set headers
        struct curl_slist* header_list = nullptr;
        header_list = add_default_headers(header_list);
        for (const auto& [key, value] : headers) {
            std::string header = key + ": " + value;
            header_list = curl_slist_append(header_list, header.c_str());
        }
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, header_list);

        // Perform request
        HttpResponse response = perform_request();

        curl_slist_free_all(header_list);
        return response;
    }

    HttpResponse get_conditional(
        const std::string& url,
        const std::string& etag,
        const std::string& last_modified) override {

        std::unordered_map<std::string, std::string> headers;
        if (!etag.empty()) {
            headers["If-None-Match"] = etag;
        }
        if (!last_modified.empty()) {
            headers["If-Modified-Since"] = last_modified;
        }
        return get(url, headers);
    }

    void set_auth_token(const std::string& token) override {
        std::lock_guard lock(m_mutex);
        m_auth_token = token;
    }

    void set_timeout(std::chrono::milliseconds timeout) override {
        std::lock_guard lock(m_mutex);
        m_timeout = timeout;
    }

private:
    void configure_curl() {
        // SSL verification
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER,
                         m_config.verify_ssl ? 1L : 0L);
        curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST,
                         m_config.verify_ssl ? 2L : 0L);

        // Timeouts
        auto timeout_ms = static_cast<long>(m_timeout.count());
        curl_easy_setopt(m_curl, CURLOPT_TIMEOUT_MS, timeout_ms);
        curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT_MS,
                         static_cast<long>(m_config.connect_timeout.count()));

        // Follow redirects
        curl_easy_setopt(m_curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(m_curl, CURLOPT_MAXREDIRS, 5L);

        // User agent
        curl_easy_setopt(m_curl, CURLOPT_USERAGENT, m_config.user_agent.c_str());

        // Response callbacks
        curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, header_callback);
    }

    struct curl_slist* add_default_headers(struct curl_slist* list) {
        // Accept header
        list = curl_slist_append(list, "Accept: application/octet-stream, application/json");

        // Auth token
        if (!m_auth_token.empty()) {
            std::string auth = "Authorization: Bearer " + m_auth_token;
            list = curl_slist_append(list, auth.c_str());
        }

        return list;
    }

    HttpResponse perform_request() {
        ResponseData data;
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &data);

        CURLcode res = curl_easy_perform(m_curl);

        HttpResponse response;
        if (res != CURLE_OK) {
            response.status_code = 0;
            response.status_message = curl_easy_strerror(res);
            return response;
        }

        // Get response code
        long code = 0;
        curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &code);
        response.status_code = static_cast<int>(code);
        response.status_message = get_status_message(response.status_code);

        // Move data
        response.body = std::move(data.body);
        response.headers = std::move(data.headers);

        return response;
    }

    static std::string get_status_message(int code) {
        switch (code) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 500: return "Internal Server Error";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            default: return "Unknown";
        }
    }

    static HttpResponse make_error_response(int code, const std::string& message) {
        HttpResponse response;
        response.status_code = code;
        response.status_message = message;
        return response;
    }

    struct ResponseData {
        std::vector<std::uint8_t> body;
        std::unordered_map<std::string, std::string> headers;
    };

    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        auto* data = static_cast<ResponseData*>(userdata);
        size_t total = size * nmemb;
        data->body.insert(data->body.end(), ptr, ptr + total);
        return total;
    }

    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
        auto* data = static_cast<ResponseData*>(userdata);
        size_t total = size * nitems;

        std::string header(buffer, total);

        // Remove trailing \r\n
        while (!header.empty() && (header.back() == '\r' || header.back() == '\n')) {
            header.pop_back();
        }

        // Parse header
        auto colon = header.find(':');
        if (colon != std::string::npos) {
            std::string key = header.substr(0, colon);
            std::string value = header.substr(colon + 1);

            // Trim whitespace
            while (!value.empty() && std::isspace(value.front())) {
                value.erase(0, 1);
            }

            data->headers[key] = value;
        }

        return total;
    }

    RemoteConfig m_config;
    CURL* m_curl;
    std::mutex m_mutex;
    std::string m_auth_token;
    std::chrono::milliseconds m_timeout{30000};
};

std::unique_ptr<IHttpClient> create_curl_client(const RemoteConfig& config) {
    return std::make_unique<CurlHttpClient>(config);
}

#else // !VOID_HAS_CURL

// =============================================================================
// Stub HTTP Client (no libcurl)
// =============================================================================

class StubHttpClient : public IHttpClient {
public:
    explicit StubHttpClient(const RemoteConfig& /*config*/) {}

    HttpResponse get(
        const std::string& /*url*/,
        const std::unordered_map<std::string, std::string>& /*headers*/) override {
        HttpResponse response;
        response.status_code = 501;
        response.status_message = "HTTP client not available (libcurl not linked)";
        return response;
    }

    HttpResponse get_conditional(
        const std::string& url,
        const std::string& etag,
        const std::string& last_modified) override {
        std::unordered_map<std::string, std::string> headers;
        if (!etag.empty()) headers["If-None-Match"] = etag;
        if (!last_modified.empty()) headers["If-Modified-Since"] = last_modified;
        return get(url, headers);
    }

    void set_auth_token(const std::string& /*token*/) override {}
    void set_timeout(std::chrono::milliseconds /*timeout*/) override {}
};

std::unique_ptr<IHttpClient> create_curl_client(const RemoteConfig& config) {
    return std::make_unique<StubHttpClient>(config);
}

#endif // VOID_HAS_CURL

} // namespace void_asset
