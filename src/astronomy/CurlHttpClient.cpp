#include "CurlHttpClient.hpp"

#include "platform/ExternalCommand.hpp"

#include <iomanip>
#include <sstream>
#include <vector>

namespace bag {

HttpResponse CurlHttpClient::get(const std::string& url, double timeoutSeconds) {
    if (url.empty() || timeoutSeconds <= 0.0) return {0, {}, "invalid curl request"};

    // argv-based curl (no shell). Horizons remains optional; hosts without curl
    // receive a clear transport error without changing astronomy semantics.
    std::ostringstream timeout;
    timeout << std::fixed << std::setprecision(0) << timeoutSeconds;

    const std::vector<std::string> argv = {
        "curl",
        "--silent",
        "--show-error",
        "--location",
        "--max-time",
        timeout.str(),
        "--write-out",
        "\n__BAGSOLAR_HTTP_STATUS__%{http_code}",
        url,
    };

    const ExternalCommandResult command = runExternalCommand(argv);
    if (!command.error.empty() && command.output.empty()) {
        return {0, {}, command.error};
    }

    const std::string& output = command.output;
    const std::string marker = "\n__BAGSOLAR_HTTP_STATUS__";
    const std::size_t markerPosition = output.rfind(marker);
    if (markerPosition == std::string::npos) {
        return {0, {}, command.exitCode == 0 ? "curl returned no HTTP status" : "curl request failed"};
    }

    const std::string body = output.substr(0, markerPosition);
    int status = 0;
    try {
        status = std::stoi(output.substr(markerPosition + marker.size()));
    } catch (...) {
        return {0, {}, "curl returned an invalid HTTP status"};
    }
    return {status, body, command.exitCode == 0 ? std::string{} : "curl request failed"};
}

} // namespace bag
