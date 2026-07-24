#include "dev_input.hpp"

#include <string_view>

#if defined(__unix__) && !defined(__EMSCRIPTEN__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace pokered {
namespace {

#if defined(__unix__) && !defined(__EMSCRIPTEN__)
bool* action(ControlButtons& controls, std::string_view name) {
    if (name == "left") return &controls.left;
    if (name == "right") return &controls.right;
    if (name == "up") return &controls.up;
    if (name == "down") return &controls.down;
    if (name == "confirm") return &controls.confirm;
    if (name == "back") return &controls.back;
    if (name == "start") return &controls.start;
    if (name == "select") return &controls.select;
    if (name == "menu") return &controls.menu;
    if (name == "quit") return &controls.quit;
    if (name == "fast_forward") return &controls.fast_forward;
    return nullptr;
}

void apply_command(std::string_view command, ControlButtons& held,
                   DevInputFrame& frame) {
    while (!command.empty() &&
           (command.back() == '\n' || command.back() == '\r'))
        command.remove_suffix(1U);
    if (command.starts_with("tap ")) {
        if (bool* value = action(frame.controls, command.substr(4U)))
            *value = true;
        return;
    }
    if (command.starts_with("down ")) {
        if (bool* value = action(held, command.substr(5U))) {
            *value = true;
            frame.controls = held;
        }
        return;
    }
    if (command.starts_with("up ")) {
        if (bool* value = action(held, command.substr(3U))) {
            *value = false;
            frame.controls = held;
        }
        return;
    }
    if (command.starts_with("text ")) {
        frame.text.append(command.substr(5U));
        return;
    }
    if (command == "erase") {
        frame.erase_text = true;
        return;
    }
    if (command == "submit") {
        frame.submit_text = true;
        return;
    }
    if (command == "toggle_player_tools") {
        frame.toggle_player_tools = true;
        return;
    }
    if (command == "toggle_developer_tools") {
        frame.toggle_developer_tools = true;
        return;
    }
    if (command == "toggle_annotations") {
        frame.toggle_world_annotations = true;
        return;
    }
    if (command == "quit")
        frame.quit = true;
}
#endif

} // namespace

DevInputSocket::~DevInputSocket() {
    close();
}

bool DevInputSocket::open(const std::filesystem::path& path,
                          std::string& error) {
#if defined(__unix__) && !defined(__EMSCRIPTEN__)
    close();
    const std::string native = path.string();
    if (native.empty() || native.size() >= sizeof(sockaddr_un::sun_path)) {
        error = "developer input socket path is empty or too long";
        return false;
    }
    descriptor_ =
        ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (descriptor_ < 0) {
        error = std::string("could not create developer input socket: ") +
                std::strerror(errno);
        return false;
    }
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, native.c_str(), native.size() + 1U);
    (void)::unlink(address.sun_path);
    if (::bind(descriptor_, reinterpret_cast<const sockaddr*>(&address),
               sizeof(address)) != 0) {
        error = std::string("could not bind developer input socket: ") +
                std::strerror(errno);
        close();
        return false;
    }
    path_ = path;
    error.clear();
    return true;
#else
    (void)path;
    error = "developer input sockets are unavailable on this platform";
    return false;
#endif
}

DevInputFrame DevInputSocket::poll() {
    DevInputFrame frame;
    frame.controls = held_;
#if defined(__unix__) && !defined(__EMSCRIPTEN__)
    if (descriptor_ < 0) return frame;
    char buffer[1024];
    for (;;) {
        const ssize_t size =
            ::recv(descriptor_, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (size < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            break;
        }
        if (size == 0) continue;
        apply_command(
            std::string_view(buffer, static_cast<std::size_t>(size)),
            held_, frame);
    }
#endif
    return frame;
}

void DevInputSocket::close() {
#if defined(__unix__) && !defined(__EMSCRIPTEN__)
    if (descriptor_ >= 0) {
        (void)::close(descriptor_);
        descriptor_ = -1;
    }
    if (!path_.empty()) {
        (void)::unlink(path_.c_str());
        path_.clear();
    }
#endif
    held_ = {};
}

} // namespace pokered
