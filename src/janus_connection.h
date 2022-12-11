#pragma once

#include "websocket_client.h"
#include <string>
#include "rtc_client.h"
#include "framegeneratorinterface.h"

#include <util/platform.h>
#include <util/threading.h>

extern "C" {
#include "janus-videoroom.h"
typedef struct janus_output MediaProvider;
}

namespace janus {
/// Only I420 raw frame is supported.
class VideoFrameGeneratorImpl : public owt::base::VideoFrameGeneratorInterface {
public:
	VideoFrameGeneratorImpl(MediaProvider *media_provider);
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
	MediaProvider *media_provider_;
};

class JanusConnection : public signaling::WebsocketClientInterface {
public:
	JanusConnection();
	~JanusConnection();

	// websocket event callbacks
	virtual void OnConnected() override;
	virtual void OnConnectionClosed(const std::string &reason) override;
	virtual void OnRecvMessage(const std::string &msg) override;

	// janus conncetion events
	void Publish(const char *url, uint32_t id, const char *display,
		     uint64_t room, const char *pin);
	void Unpublish();
	void SendOffer(std::string &sdp);

	rtc::RTCClient *GetRTCClient() const;

	void RegisterVideoProvider(MediaProvider *media_provider);

private:
	uint32_t id_;
	uint64_t room_;
	std::string display_;
	std::string pin_;
	uint64_t session_id_;
	uint64_t handle_id_;

	// send keep-alive to janus in this thread
	pthread_t keeplive_thread_;

	signaling::WebsocketClient *ws_client_;
	rtc::RTCClient *rtc_client_;
	VideoFrameGeneratorImpl *video_framer_;

	// websocket events
	void Connect(const char *url);
	void Disconnect();

	// RTCClient
	void CreateRTCClient();
	void DestoryRTCClient();

	// janus messages
	void CreateSession();
	void CreateHandle();

	int CreateKeepaliveThread();
	void CreateOffer();
	void SendCandidate(std::string &sdp, std::string &mid, int idx);
	void SetAnswer(std::string &sdp);
};
}
