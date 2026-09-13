#pragma once

#include "HttpClient.hpp"

namespace bag {

class CurlHttpClient final : public HttpClient {
public:
    HttpResponse get(const std::string& url, double timeoutSeconds) override;
};

} // namespace bag
