#include "server/websocket_server.hpp"

int main() {
    server::WebSocketServer server(3030, "data/document_snapshot.json");
    server.run();
    return 0;
}
