#pragma once

#include <memory>

#include "websocketpp/client.hpp"
#include "websocketpp/config/asio_no_tls_client.hpp"

typedef websocketpp::client<websocketpp::config::asio_client> IWebsocketClient;

namespace janus::signaling {
class WebsocketClientInterface {
 public:
  virtual void OnConnected() = 0;
  virtual void OnConnectionClosed(const std::string& reason) = 0;
  virtual void OnRecvMessage(const std::string& msg) = 0;
};

class WebsocketClient {
 public:
  WebsocketClient();
  ~WebsocketClient();

  std::string URL() const;
  bool Connected() const;

  void AddObserver(WebsocketClientInterface* observer);

  void Connect(const std::string& url);
  void Close(websocketpp::close::status::value code =
                 websocketpp::close::status::normal,
             const std::string& reason = "");
  void SendMsg(const std::string& msg);

 private:
  IWebsocketClient client_;
  WebsocketClientInterface* observer_;
  websocketpp::connection_hdl hdl_;
  websocketpp::lib::shared_ptr<websocketpp::lib::thread> thread_;

  std::string url_;
  bool connected_;

  void OnConnectionOpen();
  void OnConnectionFail();
  void OnConnectionClose();

  void OnRecvMsg(websocketpp::connection_hdl hdl,
		 IWebsocketClient::message_ptr msg);
};
}  // namespace teleport::signaling
