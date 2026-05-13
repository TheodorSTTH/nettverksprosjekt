#include "server/WebSocketServer.hpp"

#include <algorithm>
#include <boost/beast/websocket.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>

namespace server {

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

SharedRoom::SharedRoom(storage::DocumentStore store)
    : store_(std::move(store)), document_(store_.load()) {}

void SharedRoom::join(const std::shared_ptr<ClientSession>& session) {
    std::lock_guard lock(mutex_);
    sessions_.push_back(session);
    sendSnapshotLocked(session);
}

void SharedRoom::leave(const std::shared_ptr<ClientSession>& session) {
    std::lock_guard lock(mutex_);
    document_.removePresence(session->clientId());
    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(), [](const std::weak_ptr<ClientSession>& weakSession) {
            return weakSession.expired();
        }),
        sessions_.end()
    );
    broadcastSnapshotLocked();
}

void SharedRoom::handleMessage(const std::shared_ptr<ClientSession>& session, const std::string& payload) {
    std::lock_guard lock(mutex_);

    nlohmann::json message;
    try {
        message = nlohmann::json::parse(payload);
    } catch (const std::exception& error) {
        session->send(nlohmann::json({{"type", "error"}, {"message", error.what()}}).dump());
        return;
    }

    if (!message.is_object()) {
        session->send(nlohmann::json({{"type", "error"}, {"message", "Expected JSON object"}}).dump());
        return;
    }

    const std::string type = message.value("type", "");
    const std::string clientId = message.value("clientId", "");

    if (!clientId.empty()) {
        session->setClientId(clientId);
    }

    if (type == "join") {
        document_.updatePresence(message);
        broadcastSnapshotLocked();
    } else if (type == "edit:batch") {
        const bool changed = document_.applyEditBatch(message);
        if (changed) {
            saveLocked();
        }
        broadcastSnapshotLocked();
    } else if (type == "font-size:set") {
        const bool changed = document_.setFontSize(message);
        if (changed) {
            saveLocked();
        }
        broadcastSnapshotLocked();
    } else if (type == "cursor:update") {
        document_.updatePresence(message);
        broadcastSnapshotLocked();
    } else if (type == "document:request" || type == "text:request") {
        sendSnapshotLocked(session);
    } else {
        session->send(nlohmann::json({{"type", "error"}, {"message", "Unknown message type"}}).dump());
    }
}

void SharedRoom::saveLocked() {
    store_.save(document_);
}

void SharedRoom::sendSnapshotLocked(const std::shared_ptr<ClientSession>& session) {
    session->send(document_.snapshot().dump());
}

void SharedRoom::broadcastSnapshotLocked() {
    const std::string snapshot = document_.snapshot().dump();

    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(), [](const std::weak_ptr<ClientSession>& weakSession) {
            return weakSession.expired();
        }),
        sessions_.end()
    );

    for (const std::weak_ptr<ClientSession>& weakSession : sessions_) {
        if (const std::shared_ptr<ClientSession> session = weakSession.lock()) {
            session->send(snapshot);
        }
    }
}

ClientSession::ClientSession(tcp::socket socket, std::shared_ptr<SharedRoom> room)
    : websocket_(std::move(socket)), room_(std::move(room)) {}

void ClientSession::run() {
    try {
        websocket_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        websocket_.set_option(websocket::stream_base::decorator([](websocket::response_type& response) {
            response.set(beast::http::field::server, "nettverk-crdt-backend");
        }));
        websocket_.accept();

        room_->join(shared_from_this());

        while (true) {
            beast::flat_buffer buffer;
            websocket_.read(buffer);
            room_->handleMessage(shared_from_this(), beast::buffers_to_string(buffer.data()));
        }
    } catch (const std::exception& error) {
        std::cerr << "Client disconnected: " << error.what() << '\n';
    }

    room_->leave(shared_from_this());
}

void ClientSession::send(const std::string& payload) {
    std::lock_guard lock(writeMutex_);

    try {
        websocket_.text(true);
        websocket_.write(net::buffer(payload));
    } catch (const std::exception& error) {
        std::cerr << "Could not send websocket message: " << error.what() << '\n';
    }
}

void ClientSession::close() {
    std::lock_guard lock(writeMutex_);
    beast::error_code errorCode;
    websocket_.close(websocket::close_code::normal, errorCode);
}

const std::string& ClientSession::clientId() const {
    return clientId_;
}

void ClientSession::setClientId(std::string clientId) {
    clientId_ = std::move(clientId);
}

WebSocketServer::WebSocketServer(
    net::io_context& ioContext,
    std::shared_ptr<SharedRoom> room,
    unsigned short port
)
    : ioContext_(ioContext),
      room_(std::move(room)),
      acceptor_(ioContext, tcp::endpoint(tcp::v4(), port)) {}

void WebSocketServer::run() {
    std::cout << "CRDT backend listening on 0.0.0.0:" << acceptor_.local_endpoint().port() << "/ws\n";

    while (true) {
        tcp::socket socket(ioContext_);
        acceptor_.accept(socket);

        std::thread([session = std::make_shared<ClientSession>(std::move(socket), room_)] {
            session->run();
        }).detach();
    }
}

} // namespace server
