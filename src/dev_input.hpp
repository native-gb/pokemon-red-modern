#pragma once

#include "controls.hpp"

#include <filesystem>
#include <string>

namespace pokered {

struct DevInputFrame {
    ControlButtons controls;
    bool erase_text{};
    bool submit_text{};
    bool toggle_player_tools{};
    bool toggle_developer_tools{};
    bool toggle_world_annotations{};
    bool quit{};
    std::string text;
};

class DevInputSocket {
  public:
    DevInputSocket() = default;
    DevInputSocket(const DevInputSocket&) = delete;
    DevInputSocket& operator=(const DevInputSocket&) = delete;
    ~DevInputSocket();

    bool open(const std::filesystem::path& path, std::string& error);
    DevInputFrame poll();
    void close();

  private:
    std::filesystem::path path_;
    ControlButtons held_;
    int descriptor_{-1};
};

} // namespace pokered
