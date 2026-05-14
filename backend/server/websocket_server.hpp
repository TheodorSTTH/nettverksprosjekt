#pragma once

#include <cstdint>
#include <filesystem>

#include "server/document_state.hpp"

namespace server {

class WebSocketServer {
public:
    WebSocketServer(std::uint16_t port, std::filesystem::path snapshot_path);

    void run();

private:
    std::uint16_t port_;
    std::uint32_t next_peer_id_ = 1;
    DocumentState document_;
};

}  // namespace server
