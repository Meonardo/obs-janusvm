#pragma once

#include "websocket_client.h"
#include <string>
#include "rtc_client.h"
#include "framegeneratorinterface.h"

namespace janus {
/// Only I420 raw frame is supported.
class VideoFrameGeneratorImpl : public owt::base::VideoFrameGeneratorInterface {
public:
	VideoFrameGeneratorImpl();
	~VideoFrameGeneratorImpl();

	virtual uint32_t GenerateNextFrame(uint8_t *buffer,
					   const uint32_t capacity) override;
	virtual uint32_t GetNextFrameSize() override;
	virtual int GetHeight() override;
	virtual int GetWidth() override;
	virtual int GetFps() override;
	virtual owt::base::VideoFrameGeneratorInterface::VideoFrameCodec
	GetType() override;

private:
	int buffer_size_for_a_frame_;
};

class JanusConnection : public signaling::WebsocketClientInterface {
public:
	JanusConnection();
	~JanusConnection();

	// websocket events
	void Connect(const char *url);
	void Disconnect();
	// websocket event callbacks
	virtual void OnConnected() override;
	virtual void OnConnectionClosed(const std::string &reason) override;
	virtual void OnRecvMessage(const std::string &msg) override;

	// janus conncetion events
	void Publish(const char *id, const char *display, uint64_t room,
		     const char *pin);
	void Unpublish();

	// RTCClient
	void CreateRTCClient();
	void DestoryRTCClient();

private:
	std::string id_;
	uint64_t room_;
	std::string display_;
	std::string pin_;

	signaling::WebsocketClient *ws_client_;
	rtc::RTCClient *rtc_client_;
};
}
