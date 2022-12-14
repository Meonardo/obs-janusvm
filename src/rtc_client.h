#pragma once

#include "libwebrtc.h"
#include "rtc_peerconnection.h"
#include "framegeneratorinterface.h"

namespace libwebrtc {
class RTCPeerConnectionFactory;
class RTCVideoCapturer;
} // namespace libwebrtc

namespace janus::rtc {
struct RTCSessionDescription {
	std::string sdp;
	std::string type;
};

struct RTCIceCandidate {
	std::string sdp;
	int sdp_mline_index;
	std::string sdp_mid;
};

enum RTCLogLevel { kVebose = 0, kDebug, kInfo, kError, kNone };

} // namespace miracast::rtc

////////////////////////////////////////////////////////////////////////////////
// typedefs
typedef void(__stdcall *OnCreatedSdpCallback)(
	janus::rtc::RTCSessionDescription &sdp, std::string &error,
	void *params);
typedef void(__stdcall *OnCreatedIceCandidateCallback)(
	janus::rtc::RTCIceCandidate &candidate, std::string &error,
	void *params);
typedef void(__stdcall *ErrorCallback)(std::string &error, void *params);

typedef libwebrtc::RTCVideoRenderer<
	libwebrtc::scoped_refptr<libwebrtc::RTCVideoFrame>> *RTCVideoRendererPtr;

typedef std::unordered_map<std::string, std::string> ICEServer;
// end of typedefs
////////////////////////////////////////////////////////////////////////////////

namespace janus::rtc {
enum RTCMediaType {
	kVideo = 0,
	kAudio,
	kData,
};

enum RTCMediaUpdate {
	kRemoved = 0,
	kAdded,
};

class RTCClientIceCandidateObserver {
public:
	virtual void OnIceCandidateDiscoveried(std::string &id,
					       RTCIceCandidate &candidate) = 0;
};

class RTCClientConnectionObserver {
public:
	virtual void OnSignalingState(std::string &id,
				      libwebrtc::RTCSignalingState state) = 0;
	virtual void
	OnPeerConnectionState(std::string &id,
			      libwebrtc::RTCPeerConnectionState state) = 0;
	virtual void
	OnIceGatheringState(std::string &id,
			    libwebrtc::RTCIceGatheringState state) = 0;
	virtual void
	OnIceConnectionState(std::string &id,
			     libwebrtc::RTCIceConnectionState state) = 0;
};

class RTCClientMediaTrackEventObserver {
public:
	virtual void OnMediaTrackChanged(std::string &id, RTCMediaType type,
					 RTCMediaUpdate state) = 0;
};

class RTCClient : public libwebrtc::RTCPeerConnectionObserver {
public:
	RTCClient(std::string id,
		  libwebrtc::scoped_refptr<libwebrtc::RTCPeerConnectionFactory>
			  pcf,
		  std::vector<ICEServer> &ice_servers);
	~RTCClient();

	// ID
	std::string ID() const;

	// Close RTCConnection, destory any media sources
	void Close();

	// SDP
	void CreateOffer(void *params, OnCreatedSdpCallback callback);
	void CreateAnswer(void *params, OnCreatedSdpCallback callback);
	void SetLocalDescription(const char *sdp, const char *type,
				 void *params, ErrorCallback callback);
	void SetRemoteDescription(const char *sdp, const char *type,
				  void *params, ErrorCallback cb);
	void GetLocalDescription(void *params, OnCreatedSdpCallback cb);
	void GetRemoteDescription(void *params, OnCreatedSdpCallback cb);

	// ICE
	void AddCandidate(const char *mid, int mid_mline_index,
			  const char *candidate);

	// Media
	bool ToggleMute(bool mute);
	void CreateMediaSender(
		std::unique_ptr<owt::base::VideoFrameGeneratorInterface> video);

	// PC Observer & callback
	void AddPeerconnectionEventsObserver(RTCClientConnectionObserver *cb);
	void AddIceCandidateObserver(RTCClientIceCandidateObserver *cb);
	void AddMediaTrackUpdateObserver(RTCClientMediaTrackEventObserver *cb);

	virtual void
	OnSignalingState(libwebrtc::RTCSignalingState state) override;
	virtual void
	OnPeerConnectionState(libwebrtc::RTCPeerConnectionState state) override;
	virtual void
	OnIceGatheringState(libwebrtc::RTCIceGatheringState state) override;
	virtual void
	OnIceConnectionState(libwebrtc::RTCIceConnectionState state) override;
	virtual void OnIceCandidate(
		libwebrtc::scoped_refptr<libwebrtc::RTCIceCandidate> candidate)
		override;
	virtual void
	OnAddStream(libwebrtc::scoped_refptr<libwebrtc::RTCMediaStream> stream)
		override;
	virtual void OnRemoveStream(
		libwebrtc::scoped_refptr<libwebrtc::RTCMediaStream> stream)
		override;
	virtual void
	OnDataChannel(libwebrtc::scoped_refptr<libwebrtc::RTCDataChannel>
			      data_channel) override;
	virtual void OnRenegotiationNeeded() override;
	virtual void
	OnTrack(libwebrtc::scoped_refptr<libwebrtc::RTCRtpTransceiver>
			transceiver) override;
	virtual void
	OnAddTrack(libwebrtc::vector<
			   libwebrtc::scoped_refptr<libwebrtc::RTCMediaStream>>
			   streams,
		   libwebrtc::scoped_refptr<libwebrtc::RTCRtpReceiver> receiver)
		override;
	virtual void OnRemoveTrack(
		libwebrtc::scoped_refptr<libwebrtc::RTCRtpReceiver> receiver)
		override;

protected:
private:
	std::string id_;
	libwebrtc::scoped_refptr<libwebrtc::RTCVideoTrack> local_video_track_;
	libwebrtc::scoped_refptr<libwebrtc::RTCAudioTrack> audio_track_;
	libwebrtc::scoped_refptr<libwebrtc::RTCVideoTrack> remote_video_track_;
	libwebrtc::scoped_refptr<libwebrtc::RTCPeerConnectionFactory> pcf_;
	libwebrtc::scoped_refptr<libwebrtc::RTCPeerConnection> pc_;
	std::unique_ptr<libwebrtc::RTCConfiguration> rtc_config_;

	// Observers
	RTCClientConnectionObserver *events_cb_;
	RTCClientIceCandidateObserver *ice_candidate_cb_;
	RTCClientMediaTrackEventObserver *media_track_update_cb_;

	void ApplyBitrateSettings();
};

//////////////////////////////////////////////////////////////////////////////////////////
// static methods

// Update RTC log level
void UpdateRTCLogLevel(janus::rtc::RTCLogLevel level);
// Enable intel media sdk hw acc for encoding
void SetVideoHardwareAccelerationEnabled(bool enable);
// Create RTCClient
RTCClient *CreateClient(
	std::vector<std::unordered_map<std::string, std::string>> &iceServers,
	std::string id);

// end of static methods
//////////////////////////////////////////////////////////////////////////////////////////

} // namespace miracast::rtc
