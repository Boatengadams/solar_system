#pragma once

#include <filesystem>

#include "EphemerisProvider.hpp"

namespace bag {

class JsonEphemerisProvider final : public EphemerisProvider {
public:
    explicit JsonEphemerisProvider(std::filesystem::path path);

    EphemerisResult getState(const EphemerisRequest& request) override;
    const char* name() const override { return "JsonEphemerisProvider"; }

private:
    std::filesystem::path path;
};

} // namespace bag
