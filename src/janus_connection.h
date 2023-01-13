#pragma once

// include websocket_client.h file first
#include "websocket_client.h"
////////////////////////////////////////////////////////////////////////
#include "rtc_client.h"
#include "framegeneratorinterface.h"
#include "videoencoderinterface.h"

#include <util/platform.h>
#include <util/threading.h>

extern "C" {
#include "media-io/video-frame.h"
#include "media-io/audio-io.h"
#include <obs.h>
typedef struct video_frame OBSVideoFrame;
typedef struct audio_data OBSAudioFrame;
typedef struct encoder_packet OBSVideoPacket;
}

namespace janus {
// this class impl both `VideoFrameFeeder` and `VideoPacketFeeder`
// it able to send raw or encoded video data to janus video-room
class VideoFeederImpl : public owt::base::VideoFrameFeeder,
			public owt::base::VideoPacketFeeder {
public:
	VideoFeederImpl();
	~VideoFeederImpl();

	// tell the VideoFrameFeeder to store the frame's receiver(do NOT free this receiver)
	virtual void SetFrameReceiver(
		owt::base::VideoFrameReceiverInterface *receiver) override;
	// call this function from obs
	void FeedVideoFrame(OBSVideoFrame *frame, int width, int height);

	// encoded packet
	virtual void SetBufferReceiver(
		owt::base::VideoPacketReceiverInterface *receiver) override;
	// call this function from obs
	void FeedVideoPacket(OBSVideoPacket *pkt, int width, int height);

private:
	owt::base::VideoFrameReceiverInterface *frame_receiver_;
	owt::base::VideoPacketReceiverInterface *packet_receiver_;
};

class AudioFeederImpl : public owt::base::AudioFrameFeeder {
public:
	AudioFeederImpl();
	~AudioFeederImpl();

	/// Get sample rate for frames generated.
	virtual int GetSampleRate() override;
	/// Get numbers of channel for frames generated.
	virtual int GetChannelNumber() override;
	/// pass the receiver to whom will send audio frames
	virtual void SetAudioFrameReceiver(
		owt::base::AudioFrameReceiverInterface *receiver) override;

	// call this function from obs
	void FeedAudioFrame(OBSAudioFrame *frame);

private:
	owt::base::AudioFrameReceiverInterface *frame_receiver_;
	size_t channels_;
	size_t audio_bytes_per_channel_; // webrtc frame byte size: sizeof(int16_t) = 2
	uint32_t sample_rate_;
};

class JanusConnection : public signaling::WebsocketClientInterface,
			public rtc::RTCClientIceCandidateObserver {
public:
	JanusConnection(bool send_encoded_data);
	~JanusConnection();

	// websocket event callbacks
	virtual void OnConnected() override;
	virtual void OnConnectionClosed(const std::string &reason) override;
	virtual void OnRecvMessage(const std::string &msg) override;

	// RTCClient event callbacks
	virtual void
	OnIceCandidateDiscoveried(std::string &id,
				  rtc::RTCIceCandidate &candidate) override;

	// janus conncetion events
	void Publish(const char *url, uint32_t id, const char *display,
		     uint64_t room, const char *pin);
	void Unpublish();
	void SendOffer(std::string &sdp);

	rtc::RTCClient *GetRTCClient() const;

	// called from obs output
	void SendVideoFrame(OBSVideoFrame *frame, int width, int height);
	void SendVideoPacket(OBSVideoPacket *pkt, int width, int height);
	void SendAudioFrame(OBSAudioFrame *frame);

private:
	bool use_encoded_data_;
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
	VideoFeederImpl *video_feeder_;
	std::shared_ptr<AudioFeederImpl> audio_feeder_;

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
