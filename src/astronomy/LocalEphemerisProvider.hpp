#pragma once

#include <map>

#include "EphemerisProvider.hpp"

namespace bag {

class LocalEphemerisProvider final : public EphemerisProvider {
public:
    explicit LocalEphemerisProvider(std::vector<EphemerisState> states = {});

    EphemerisResult getState(const EphemerisRequest& request) override;
    const char* name() const override { return "LocalEphemerisProvider"; }
    static LocalEphemerisProvider deterministicFixture();

private:
    std::map<std::string, EphemerisState> states;
};

} // namespace bag
