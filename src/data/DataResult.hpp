#pragma once

#include <optional>
#include <string>
#include <utility>

namespace bag {

template <typename T>
struct DataResult {
    std::optional<T> value;
    std::string error;

    static DataResult success(T result) { return {std::move(result), {}}; }
    static DataResult failure(std::string message) { return {std::nullopt, std::move(message)}; }
    explicit operator bool() const { return value.has_value(); }
};

} // namespace bag
