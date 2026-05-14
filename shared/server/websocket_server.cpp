#include "server/websocket_server.hpp"

#include <iostream>
#include <string>
#include <string_view>

#include <App.h>
#include <nlohmann/json.hpp>

#include "protocol/message.hpp"

namespace server {
namespace {

struct ClientData {
    std::uint32_t peer_id = 0;
};

std::string dump_message(const nlohmann::json& message) {
    return message.dump();
}

}  // namespace

WebSocketServer::WebSocketServer(std::uint16_t port, std::filesystem::path snapshot_path)
    : port_(port), document_(0, std::move(snapshot_path)) {}

void WebSocketServer::run() {
    document_.load();

    uWS::App app;

    app.ws<ClientData>(
           "/ws",
           {
               .open =
                   [this](auto* ws) {
                       auto* data = ws->getUserData();
                       data->peer_id = next_peer_id_++;
                       ws->subscribe("document");

                       ws->send(dump_message(protocol::serialize_assign_peer_id(data->peer_id)), uWS::OpCode::TEXT);
                       ws->send(dump_message(document_.snapshot_json()), uWS::OpCode::TEXT);
                   },
               .message =
                   [this](auto* ws, std::string_view raw_message, uWS::OpCode) {
                       nlohmann::json message;
                       try {
                           message = nlohmann::json::parse(raw_message);
                       } catch (...) {
                           ws->send(dump_message(protocol::serialize_error("invalid json")), uWS::OpCode::TEXT);
                           return;
                       }

                       const auto validation = protocol::validate_client_message(message);
                       if (!validation.ok) {
                           ws->send(dump_message(protocol::serialize_error(validation.error)), uWS::OpCode::TEXT);
                           return;
                       }

                       switch (protocol::message_type(message)) {
                           case protocol::MessageType::SyncRequest:
                               ws->send(dump_message(document_.snapshot_json()), uWS::OpCode::TEXT);
                               return;

                           case protocol::MessageType::Insert: {
                               const auto operation = protocol::parse_insert(message);
                               const auto result = document_.apply_insert(*operation);
                               if (result.status == ApplyStatus::MissingParent) {
                                   ws->send(dump_message(protocol::serialize_error("insert parent does not exist")),
                                            uWS::OpCode::TEXT);
                                   ws->send(dump_message(document_.snapshot_json()), uWS::OpCode::TEXT);
                                   return;
                               }
                               if (result.status == ApplyStatus::Applied) {
                                   document_.save();
                                   ws->publish("document",
                                               dump_message(protocol::serialize_insert(
                                                   result.operation, protocol::MessageType::RemoteInsert)),
                                               uWS::OpCode::TEXT);
                               }
                               return;
                           }

                           case protocol::MessageType::Delete: {
                               const auto operation = protocol::parse_delete(message);
                               const auto result = document_.apply_delete(*operation);
                               if (result.status == ApplyStatus::MissingTarget) {
                                   ws->send(dump_message(protocol::serialize_error("delete target does not exist")),
                                            uWS::OpCode::TEXT);
                                   ws->send(dump_message(document_.snapshot_json()), uWS::OpCode::TEXT);
                                   return;
                               }
                               if (result.status == ApplyStatus::Applied) {
                                   document_.save();
                                   ws->publish("document",
                                               dump_message(protocol::serialize_delete(
                                                   result.operation, protocol::MessageType::RemoteDelete)),
                                               uWS::OpCode::TEXT);
                               }
                               return;
                           }

                           default:
                               ws->send(dump_message(protocol::serialize_error("unsupported message type")),
                                        uWS::OpCode::TEXT);
                               return;
                       }
                   },
           })
        .listen(port_, [this](auto* token) {
            if (token) {
                std::cout << "CRDT server listening on ws://localhost:" << port_ << "/ws\n";
            } else {
                std::cerr << "Failed to listen on port " << port_ << '\n';
            }
        })
        .run();
}

}  // namespace server
