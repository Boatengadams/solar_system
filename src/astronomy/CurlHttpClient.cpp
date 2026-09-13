#include "CurlHttpClient.hpp"

#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace bag {
namespace {
std::string shellQuote(const std::string& value) {
    std::string quoted = "'";
    for (char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    return quoted + "'";
}
}

HttpResponse CurlHttpClient::get(const std::string& url, double timeoutSeconds) {
    if (url.empty() || timeoutSeconds <= 0.0) return {0, {}, "invalid curl request"};
    std::ostringstream command;
    command << "curl --silent --show-error --location --max-time " << std::fixed << std::setprecision(0) << timeoutSeconds
            << " --write-out '\\n__BAGSOLAR_HTTP_STATUS__%{http_code}' " << shellQuote(url) << " 2>/dev/null";
    FILE* pipe = popen(command.str().c_str(), "r");
    if (!pipe) return {0, {}, "could not start curl"};
    std::string output;
    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) output += buffer;
    const int result = pclose(pipe);
    const std::string marker = "\n__BAGSOLAR_HTTP_STATUS__";
    const std::size_t markerPosition = output.rfind(marker);
    if (markerPosition == std::string::npos) return {0, {}, result == 0 ? "curl returned no HTTP status" : "curl request failed"};
    const std::string body = output.substr(0, markerPosition);
    int status = 0;
    try { status = std::stoi(output.substr(markerPosition + marker.size())); }
    catch (...) { return {0, {}, "curl returned an invalid HTTP status"}; }
    return {status, body, result == 0 ? std::string{} : "curl request failed"};
}

} // namespace bag
