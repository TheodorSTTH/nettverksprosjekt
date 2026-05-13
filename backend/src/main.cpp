#include "server/WebSocketServer.hpp"
#include "storage/DocumentStore.hpp"

#include <boost/asio.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace {

unsigned short configuredPort() {
    const char* port = std::getenv("PORT");

    if (!port) {
        return 3030;
    }

    return static_cast<unsigned short>(std::stoi(port));
}

std::filesystem::path configuredStorePath() {
    const char* path = std::getenv("DOCUMENT_STORE");

    if (!path) {
        return "data/document.json";
    }

    return path;
}

} // namespace

int main() {
    try {
        boost::asio::io_context ioContext;
        auto room = std::make_shared<server::SharedRoom>(storage::DocumentStore(configuredStorePath()));
        server::WebSocketServer server(ioContext, room, configuredPort());
        server.run();
    } catch (const std::exception& error) {
        std::cerr << "Backend failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
