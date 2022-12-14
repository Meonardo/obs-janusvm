#pragma once

#include "websocket_client.h"
#include "rtc_client.h"
#include "framegeneratorinterface.h"

#include <util/platform.h>
#include <util/threading.h>

extern "C" {
#include "media-io/video-frame.h"
typedef struct video_frame OBSVideoFrame;
}

namespace janus {
class VideoFrameFeederImpl : public owt::base::VideoFrameFeeder {
public:
	VideoFrameFeederImpl();
	~VideoFrameFeederImpl();

	// tell the framegenerator to store the frame's receiver
	virtual void SetFrameReceiver(owt::base::VideoFrameReceiverInterface *receiver);
	// call this function from obs
	void FeedVideoFrame(OBSVideoFrame *frame, int width, int height);

private:
	owt::base::VideoFrameReceiverInterface *frame_receiver_;
};

class JanusConnection : public signaling::WebsocketClientInterface, public rtc::RTCClientIceCandidateObserver {
public:
	JanusConnection();
	~JanusConnection();

	// websocket event callbacks
	virtual void OnConnected() override;
	virtual void OnConnectionClosed(const std::string &reason) override;
	virtual void OnRecvMessage(const std::string &msg) override;

	// RTCClient event callbacks
	virtual void OnIceCandidateDiscoveried(std::string &id, rtc::RTCIceCandidate &candidate) override;

	// janus conncetion events
	void Publish(const char *url, uint32_t id, const char *display,
		     uint64_t room, const char *pin);
	void Unpublish();
	void SendOffer(std::string &sdp);

	rtc::RTCClient *GetRTCClient() const;

	// called from obs output
	void SendVideoFrame(OBSVideoFrame *frame, int width, int height);

private:
	uint32_t id_;
	uint64_t room_;
	std::string display_;
	std::string pin_;
	uint64_t session_id_;
	uint64_t handle_id_;
	bool joined_room_;

	// send keep-alive to janus in this thread
	pthread_t keeplive_thread_;

	signaling::WebsocketClient *ws_client_;
	rtc::RTCClient *rtc_client_;
	VideoFrameFeederImpl *video_framer_;

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
	void InvalidKeepaliveThread();

	void CreateOffer();
	void SendCandidate(std::string &sdp, std::string &mid, int idx);
	void SetAnswer(std::string &sdp);
};
}
