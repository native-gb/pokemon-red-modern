#include "content/catalog.hpp"

namespace pokered::content {

const char* label(PackState state) {
    switch (state) {
    case PackState::absent:
        return "Not imported";
    case PackState::importing:
        return "Importing";
    case PackState::ready:
        return "Ready";
    case PackState::incompatible:
        return "Incompatible";
    }
    return "Unknown";
}

} // namespace pokered::content
