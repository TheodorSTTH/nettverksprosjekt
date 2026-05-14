#include "server/websocket_server.hpp"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <App.h>
#include <nlohmann/json.hpp>

#include "protocol/message.hpp"
#include "server/document_state.hpp"

namespace server {
namespace {

struct ClientData {
    std::uint32_t peer_id = 0;
};

using WebSocket = uWS::WebSocket<false, true, ClientData>;
using ConnectedClients = std::unordered_map<std::uint32_t, WebSocket*>;

constexpr auto kClientUpdateInterval = std::chrono::seconds(2);

std::string dump_message(const nlohmann::json& message) {
    return message.dump();
}

class DelayedOperationApplier {
public:
    DelayedOperationApplier(
        uWS::Loop* loop,
        DocumentState& document,
        ConnectedClients& clients
    )
        : loop_(loop),
          document_(document),
          clients_(clients),
          worker_([this] { run(); }) {}

    ~DelayedOperationApplier() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }

        condition_.notify_one();

        if (worker_.joinable()) {
            worker_.join();
        }
    }

    void enqueue(std::uint32_t sender_peer_id, nlohmann::json message) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            messages_.push(QueuedMessage{sender_peer_id, std::move(message)});
        }

        condition_.notify_one();
    }

private:
    struct QueuedMessage {
        std::uint32_t sender_peer_id;
        nlohmann::json message;
    };

    void run() {
        while (true) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this] {
                    return stopped_ || !messages_.empty();
                });

                if (stopped_ && messages_.empty()) {
                    return;
                }
            }

            // Wait before applying anything to the server document.
            std::this_thread::sleep_for(kClientUpdateInterval);

            std::vector<QueuedMessage> batch;

            {
                std::lock_guard<std::mutex> lock(mutex_);

                while (!messages_.empty()) {
                    batch.push_back(std::move(messages_.front()));
                    messages_.pop();
                }
            }

            loop_->defer([this, batch = std::move(batch)]() mutable {
                apply_batch(batch);
            });
        }
    }

    void apply_batch(const std::vector<QueuedMessage>& batch) {
        bool changed = false;

        for (const auto& queued : batch) {
            switch (protocol::message_type(queued.message)) {
                case protocol::MessageType::Insert: {
                    const auto operation = protocol::parse_insert(queued.message);

                    if (!operation) {
                        send_to_peer(
                            queued.sender_peer_id,
                            dump_message(protocol::serialize_error("invalid insert message"))
                        );
                        continue;
                    }

                    const auto result = document_.apply_insert(*operation);

                    if (result.status == ApplyStatus::MissingParent) {
                        send_to_peer(
                            queued.sender_peer_id,
                            dump_message(protocol::serialize_error("insert parent does not exist"))
                        );

                        send_to_peer(
                            queued.sender_peer_id,
                            dump_message(document_.snapshot_json())
                        );

                        continue;
                    }

                    if (result.status == ApplyStatus::Applied) {
                        changed = true;

                        broadcast_except(
                            queued.sender_peer_id,
                            dump_message(protocol::serialize_insert(
                                result.operation,
                                protocol::MessageType::RemoteInsert
                            ))
                        );
                    }

                    break;
                }

                case protocol::MessageType::Delete: {
                    const auto operation = protocol::parse_delete(queued.message);

                    if (!operation) {
                        send_to_peer(
                            queued.sender_peer_id,
                            dump_message(protocol::serialize_error("invalid delete message"))
                        );
                        continue;
                    }

                    const auto result = document_.apply_delete(*operation);

                    if (result.status == ApplyStatus::MissingTarget) {
                        send_to_peer(
                            queued.sender_peer_id,
                            dump_message(protocol::serialize_error("delete target does not exist"))
                        );

                        send_to_peer(
                            queued.sender_peer_id,
                            dump_message(document_.snapshot_json())
                        );

                        continue;
                    }

                    if (result.status == ApplyStatus::Applied) {
                        changed = true;

                        broadcast_except(
                            queued.sender_peer_id,
                            dump_message(protocol::serialize_delete(
                                result.operation,
                                protocol::MessageType::RemoteDelete
                            ))
                        );
                    }

                    break;
                }

                default:
                    send_to_peer(
                        queued.sender_peer_id,
                        dump_message(protocol::serialize_error("unsupported message type"))
                    );
                    break;
            }
        }

        if (changed) {
            document_.save();
        }
    }

    void send_to_peer(std::uint32_t peer_id, const std::string& message) {
        if (auto client = clients_.find(peer_id); client != clients_.end()) {
            client->second->send(message, uWS::OpCode::TEXT);
        }
    }

    void broadcast_except(std::uint32_t sender_peer_id, const std::string& message) {
        for (const auto& [peer_id, client] : clients_) {
            if (peer_id != sender_peer_id) {
                client->send(message, uWS::OpCode::TEXT);
            }
        }
    }

    uWS::Loop* loop_;
    DocumentState& document_;
    ConnectedClients& clients_;

    std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<QueuedMessage> messages_;

    bool stopped_ = false;
    std::thread worker_;
};

}  // namespace

WebSocketServer::WebSocketServer(std::uint16_t port, std::filesystem::path snapshot_path)
    : port_(port), document_(0, std::move(snapshot_path)) {}

void WebSocketServer::run() {
    document_.load();

    uWS::App app;
    ConnectedClients connected_clients;

    DelayedOperationApplier delayed_operations(
        app.getLoop(),
        document_,
        connected_clients
    );

    app.ws<ClientData>(
           "/ws",
           {
               .open =
                   [this, &connected_clients](auto* ws) {
                       auto* data = ws->getUserData();

                       data->peer_id = next_peer_id_++;
                       connected_clients[data->peer_id] = ws;

                       ws->subscribe("document");

                       ws->send(
                           dump_message(protocol::serialize_assign_peer_id(data->peer_id)),
                           uWS::OpCode::TEXT
                       );

                       ws->send(
                           dump_message(document_.snapshot_json()),
                           uWS::OpCode::TEXT
                       );
                   },

               .message =
                   [this, &delayed_operations](auto* ws, std::string_view raw_message, uWS::OpCode) {
                       nlohmann::json message;

                       try {
                           message = nlohmann::json::parse(raw_message);
                       } catch (...) {
                           ws->send(
                               dump_message(protocol::serialize_error("invalid json")),
                               uWS::OpCode::TEXT
                           );
                           return;
                       }

                       const auto validation = protocol::validate_client_message(message);

                       if (!validation.ok) {
                           ws->send(
                               dump_message(protocol::serialize_error(validation.error)),
                               uWS::OpCode::TEXT
                           );
                           return;
                       }

                       switch (protocol::message_type(message)) {
                           case protocol::MessageType::SyncRequest:
                               ws->send(
                                   dump_message(document_.snapshot_json()),
                                   uWS::OpCode::TEXT
                               );
                               return;

                           case protocol::MessageType::Insert:
                               delayed_operations.enqueue(
                                   ws->getUserData()->peer_id,
                                   message
                               );
                               return;

                           case protocol::MessageType::Delete:
                               delayed_operations.enqueue(
                                   ws->getUserData()->peer_id,
                                   message
                               );
                               return;

                           default:
                               ws->send(
                                   dump_message(protocol::serialize_error("unsupported message type")),
                                   uWS::OpCode::TEXT
                               );
                               return;
                       }
                   },

               .close =
                   [&connected_clients](auto* ws, int, std::string_view) {
                       connected_clients.erase(ws->getUserData()->peer_id);
                   },
           })
        .listen(port_, [this](auto* token) {
            if (token) {
                std::cout << "CRDT server listening on ws://localhost:" << port_ << "/ws" << std::endl;
                std::cout << "Delaying CRDT client updates by 10 seconds" << std::endl;
            } else {
                std::cerr << "Failed to listen on port " << port_ << '\n';
            }
        })
        .run();
}

}  // namespace server
