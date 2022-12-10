#include "janus_connection.h"

namespace janus {

VideoFrameGeneratorImpl::VideoFrameGeneratorImpl() : buffer_size_for_a_frame_(0)
{
}
VideoFrameGeneratorImpl::~VideoFrameGeneratorImpl() {}

uint32_t VideoFrameGeneratorImpl::GenerateNextFrame(uint8_t *buffer,
						    const uint32_t capacity)
{
	return 0;
}

uint32_t VideoFrameGeneratorImpl::GetNextFrameSize()
{
	if (buffer_size_for_a_frame_ == 0) {
		int size = GetWidth() * GetHeight();
		int qsize = size / 4;
		buffer_size_for_a_frame_ = size + 2 * qsize;
	}
	return buffer_size_for_a_frame_;
}

int VideoFrameGeneratorImpl::GetHeight()
{
	return 1080;
}

int VideoFrameGeneratorImpl::GetWidth()
{
	return 1920;
}

int VideoFrameGeneratorImpl::GetFps()
{
	return 30;
}

owt::base::VideoFrameGeneratorInterface::VideoFrameCodec
VideoFrameGeneratorImpl::GetType()
{
	return owt::base::VideoFrameGeneratorInterface::VideoFrameCodec::I420;
}

/////////////////////////////////////////////////////////////////////////////////

JanusConnection::JanusConnection() : ws_client_(nullptr), rtc_client_(nullptr)
{
}

JanusConnection::~JanusConnection()
{
	Disconnect();
	DestoryRTCClient();
}

void JanusConnection::Connect(const char *url)
{
	if (ws_client_ == nullptr) {
		ws_client_ = new signaling::WebsocketClient();
		ws_client_->AddObserver(this);
	}
	ws_client_->Connect(url);
}

void JanusConnection::Disconnect()
{
	if (ws_client_ == nullptr)
		return;

	ws_client_->Close();
	delete ws_client_;
	ws_client_ = nullptr;
}

void JanusConnection::OnConnected() {}

void JanusConnection::OnConnectionClosed(const std::string &reason) {}

void JanusConnection::OnRecvMessage(const std::string &msg) {}

void JanusConnection::Publish(const char *id, const char *display,
			      uint64_t room, const char *pin)
{
	id_ = id;
	display_ = display;
	room_ = room;
	pin_ = pin;
}

void JanusConnection::Unpublish() {}

void JanusConnection::CreateRTCClient()
{
	if (rtc_client_ != nullptr)
		return;

	typedef std::unordered_map<std::string, std::string> ICEServer;
	auto ice_servers = std::vector<ICEServer>{};
	// default ice servers
	{
		ICEServer map = {
			{"uri", "stun:192.168.99.48:3478"},
			{"username", "root"},
			{"passwd", "123456"},
		};
		ice_servers.push_back(map);
	}
	{
		ICEServer map = {
			{"uri", "turn:192.168.99.48:3478"},
			{"username", "root"},
			{"passwd", "123456"},
		};
		ice_servers.push_back(map);
	}
	rtc_client_ = rtc::CreateClient(ice_servers, "obs");
}

void JanusConnection::DestoryRTCClient()
{
	if (rtc_client_ == nullptr)
		return;

	rtc_client_->Close();
	delete rtc_client_;
	rtc_client_ = nullptr;
}

}
