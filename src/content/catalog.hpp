#pragma once

#include <cstddef>
#include <string_view>

namespace pokered::content {

enum class PackState {
    absent,
    importing,
    ready,
    incompatible,
};

struct CatalogSummary {
    PackState state{PackState::absent};
    std::string_view campaign{"No campaign loaded"};
    std::string_view source{"No local cartridge imported"};
    std::size_t maps{};
    std::size_t scripts{};
    std::size_t species{};
    std::size_t moves{};
    std::size_t items{};
};

const char* label(PackState state);

} // namespace pokered::content
