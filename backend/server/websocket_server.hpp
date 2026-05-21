#pragma once

#include <cstdint>
#include <filesystem>

#include "server/document_state.hpp"

namespace server {

/**
 * @brief WebSocket entry point for the collaborative editor.
 *
 * Built on uWebSockets, which runs all I/O on a single event loop. A
 * background worker delays incoming operations by kClientUpdateInterval
 * to make concurrent edits observable, then defers the batch back to the
 * event loop. The authoritative state is owned by an internal
 * DocumentState that persists to a JSON snapshot on every change.
 */
class WebSocketServer {
public:
    /**
     * @brief Construct the server.
     * @param port           TCP port to listen on.
     * @param snapshot_path  Path used by DocumentState for persistence.
     */
    WebSocketServer(std::uint16_t port, std::filesystem::path snapshot_path);

    /**
     * @brief Load the snapshot and run the event loop. Blocks until the loop exits.
     */
    void run();

private:
    std::uint16_t port_;
    std::uint32_t next_peer_id_ = 1;  ///< Assigned on every new connection.
    DocumentState document_;
};

}  // namespace server
