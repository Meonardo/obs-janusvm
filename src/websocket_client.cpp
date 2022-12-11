#include "websocket_client.h"
#include "janus-videoroom.h"

namespace janus::signaling {
WebsocketClient::WebsocketClient()
	: connected_(false), url_(""), observer_(nullptr), thread_(nullptr)
{
	client_.clear_access_channels(websocketpp::log::alevel::all);
	client_.clear_error_channels(websocketpp::log::alevel::all);

	client_.init_asio();
	client_.start_perpetual();

	thread_.reset(
		new websocketpp::lib::thread(&IWebsocketClient::run, &client_));
}

WebsocketClient::~WebsocketClient()
{
	client_.stop_perpetual();

	if (connected_)
		Close();

	thread_->join();

	blog(LOG_DEBUG, "");
}

std::string WebsocketClient::URL() const
{
	return url_;
}

bool WebsocketClient::Connected() const
{
	return connected_;
}

void WebsocketClient::AddObserver(WebsocketClientInterface *observer)
{
	observer_ = observer;
}

void WebsocketClient::Connect(const std::string &url)
{
	websocketpp::lib::error_code ec;
	IWebsocketClient::connection_ptr conn = client_.get_connection(url, ec);
	if (ec) {
		blog(LOG_DEBUG, "init connection error: %s",
		     ec.message().c_str());
		return;
	}
	url_ = url;

	try {
		hdl_ = conn->get_handle();
		conn->add_subprotocol("janus-protocol");

		conn->set_open_handler(websocketpp::lib::bind(
			&WebsocketClient::OnConnectionOpen, this));
		conn->set_close_handler(websocketpp::lib::bind(
			&WebsocketClient::OnConnectionClose, this));
		conn->set_fail_handler(websocketpp::lib::bind(
			&WebsocketClient::OnConnectionFail, this));
		conn->set_message_handler(websocketpp::lib::bind(
			&WebsocketClient::OnRecvMsg, this,
			websocketpp::lib::placeholders::_1,
			websocketpp::lib::placeholders::_2));

		client_.connect(conn);
	} catch (const std::exception &e) {
		blog(LOG_DEBUG, "%s", e.what());
	} catch (websocketpp::lib::error_code e) {
		blog(LOG_DEBUG, "%s", e.message().c_str());
	} catch (...) {
		blog(LOG_DEBUG, "other exception");
	}
}

void WebsocketClient::Close(websocketpp::close::status::value code,
			    const std::string &reason)
{
	try {
		client_.close(hdl_, code, reason);
	} catch (const std::exception &e) {
		blog(LOG_DEBUG, "%s", e.what());
	} catch (websocketpp::lib::error_code e) {
		blog(LOG_DEBUG, "%s", e.message().c_str());
	} catch (...) {
		blog(LOG_DEBUG, "other exception");
	}
}

void WebsocketClient::SendMsg(const std::string &msg)
{
	blog(LOG_DEBUG, "\n>>>>>>>>>>>>>>>>>>>>>>>>>\n%s", msg.c_str());
	client_.send(hdl_, msg, websocketpp::frame::opcode::text);
}

void WebsocketClient::OnConnectionOpen()
{
	blog(LOG_DEBUG, "Connection opened");
	connected_ = true;
	observer_->OnConnected();
}

void WebsocketClient::OnConnectionFail()
{
	connected_ = false;
	IWebsocketClient::connection_ptr con = client_.get_con_from_hdl(hdl_);

	std::string server = con->get_response_header("Server");
	std::string error_reason = con->get_ec().message();

	blog(LOG_DEBUG, "connection failed, server info: %s, reason: %s",
	     server.c_str(), error_reason.c_str());
	if (observer_)
		observer_->OnConnectionClosed(error_reason);
}

void WebsocketClient::OnConnectionClose()
{
	connected_ = false;
	IWebsocketClient::connection_ptr con = client_.get_con_from_hdl(hdl_);
	std::stringstream s;
	s << "close code: " << con->get_remote_close_code() << " ("
	  << websocketpp::close::status::get_string(
		     con->get_remote_close_code())
	  << "), close reason: " << con->get_remote_close_reason();
	blog(LOG_DEBUG, "%s", s.str().c_str());
	if (observer_)
		observer_->OnConnectionClosed(s.str());
}

void WebsocketClient::OnRecvMsg(websocketpp::connection_hdl hdl,
				IWebsocketClient::message_ptr msg)
{
	if (msg->get_opcode() == websocketpp::frame::opcode::text) {
		blog(LOG_DEBUG, "\n<<<<<<<<<<<<<<<<<<<<<<<<<\n%s",
		     msg->get_payload().c_str());
		std::string payload = msg->get_payload();
		observer_->OnRecvMessage(payload);
	} else {
		blog(LOG_DEBUG, "\n<<<<<<<<<<<<<<<<<<<<<<<<<\n%s",
		     websocketpp::utility::to_hex(msg->get_payload()));
	}
}

} // namespace teleport::signaling
