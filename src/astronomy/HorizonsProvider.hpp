#pragma once

#include <map>
#include <memory>

#include "EphemerisProvider.hpp"
#include "HorizonsRequest.hpp"
#include "HttpClient.hpp"

namespace bag {

class HorizonsProvider final : public EphemerisProvider {
public:
    explicit HorizonsProvider(std::shared_ptr<HttpClient> client = std::make_shared<UnavailableHttpClient>(), double timeoutSeconds = 15.0);

    EphemerisResult getState(const EphemerisRequest& request) override;
    const char* name() const override { return "HorizonsProvider"; }
    void clearCache();

private:
    std::shared_ptr<HttpClient> client;
    double timeoutSeconds;
    std::map<std::string, EphemerisState> cache;
};

} // namespace bag
