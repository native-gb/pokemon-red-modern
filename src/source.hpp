#pragma once

#include <cstddef>
#include <string>

namespace pokered {

struct SourceSpan {
    std::string file;
    std::size_t line{1};
    std::size_t column{1};

    bool operator==(const SourceSpan&) const = default;
};

} // namespace pokered
