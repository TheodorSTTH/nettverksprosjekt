#pragma once

#include "server/CollaborativeDocument.hpp"
#include "storage/DocumentStore.hpp"

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace server {

class ClientSession;

class SharedRoom : public std::enable_shared_from_this<SharedRoom> {
public:
    explicit SharedRoom(storage::DocumentStore store);

    void join(const std::shared_ptr<ClientSession>& session);
    void leave(const std::shared_ptr<ClientSession>& session);
    void handleMessage(const std::shared_ptr<ClientSession>& session, const std::string& payload);

private:
    void saveLocked();
    void sendSnapshotLocked(const std::shared_ptr<ClientSession>& session);
    void broadcastSnapshotLocked();

    storage::DocumentStore store_;
    CollaborativeDocument document_;
    std::mutex mutex_;
    std::vector<std::weak_ptr<ClientSession>> sessions_;
};

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    using WebSocket = boost::beast::websocket::stream<boost::beast::tcp_stream>;

    ClientSession(boost::asio::ip::tcp::socket socket, std::shared_ptr<SharedRoom> room);

    void run();
    void send(const std::string& payload);
    void close();

    [[nodiscard]] const std::string& clientId() const;
    void setClientId(std::string clientId);

private:
    WebSocket websocket_;
    std::shared_ptr<SharedRoom> room_;
    std::mutex writeMutex_;
    std::string clientId_;
};

class WebSocketServer {
public:
    WebSocketServer(
        boost::asio::io_context& ioContext,
        std::shared_ptr<SharedRoom> room,
        unsigned short port
    );

    void run();

private:
    boost::asio::io_context& ioContext_;
    std::shared_ptr<SharedRoom> room_;
    boost::asio::ip::tcp::acceptor acceptor_;
};

} // namespace server
