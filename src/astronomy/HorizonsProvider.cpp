#include "HorizonsProvider.hpp"

#include "HorizonsParser.hpp"

namespace bag {

HorizonsProvider::HorizonsProvider(std::shared_ptr<HttpClient> httpClient, double timeout)
    : client(std::move(httpClient)), timeoutSeconds(timeout) {}

EphemerisResult HorizonsProvider::getState(const EphemerisRequest& request) {
    if (request.bodyId.empty() || !request.epoch.valid()) return {EphemerisStatus::INVALID_REQUEST, {}, "body and a valid Julian Date are required"};
    if (!request.frame.valid()) return {EphemerisStatus::UNSUPPORTED_FRAME, {}, "requested frame is unsupported"};
    std::string mappedBody;
    if (!horizonsBodyId(request.bodyId, mappedBody)) return {EphemerisStatus::BODY_NOT_FOUND, {}, "unsupported Horizons body '" + request.bodyId + "'"};
    HorizonsRequest horizonsRequest;
    std::string error;
    if (!horizonsRequestFor(request, horizonsRequest, error)) {
        return {EphemerisStatus::INVALID_REQUEST, {}, error};
    }
    const auto cached = cache.find(horizonsRequest.url);
    if (cached != cache.end()) return {EphemerisStatus::SUCCESS, cached->second, {}};
    if (!client) return {EphemerisStatus::PROVIDER_UNAVAILABLE, {}, "Horizons HTTP client is unavailable"};
    const HttpResponse response = client->get(horizonsRequest.url, timeoutSeconds);
    if (response.statusCode == 0) return {EphemerisStatus::NETWORK_ERROR, {}, response.error.empty() ? "Horizons network request failed" : response.error};
    if (response.statusCode == 408) return {EphemerisStatus::TIMEOUT, {}, "Horizons request timed out"};
    if (response.statusCode < 200 || response.statusCode >= 300) return {EphemerisStatus::REMOTE_ERROR, {}, "Horizons HTTP status " + std::to_string(response.statusCode)};
    EphemerisResult result = parseHorizonsResponse(response.body, request);
    if (result) cache.emplace(horizonsRequest.url, result.state);
    return result;
}

void HorizonsProvider::clearCache() { cache.clear(); }

} // namespace bag
