#pragma once

#include <string>

namespace bag {

struct HttpResponse {
    int statusCode = 0;
    std::string body;
    std::string error;
};

class HttpClient {
public:
    virtual ~HttpClient() = default;
    virtual HttpResponse get(const std::string& url, double timeoutSeconds) = 0;
};

class UnavailableHttpClient final : public HttpClient {
public:
    HttpResponse get(const std::string&, double) override {
        return {0, {}, "no HTTP transport is enabled in this build"};
    }
};

} // namespace bag
