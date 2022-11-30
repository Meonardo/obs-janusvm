#include "rtc_client.h"
#include "rtc_audio_device.h"
#include "rtc_audio_source.h"
#include "rtc_desktop_device.h"
#include "rtc_peerconnection_factory.h"
#include "janus-videoroom.h"

using namespace libwebrtc;

namespace janus::rtc {

static scoped_refptr<RTCPeerConnectionFactory> g_pcf_ = nullptr;

RTCClient::RTCClient(std::string id,
		     scoped_refptr<RTCPeerConnectionFactory> pcf,
		     const RTCConfiguration &configuration,
		     scoped_refptr<RTCMediaConstraints> constraints)
	: pcf_(pcf),
	  pc_(pcf->Create(configuration, constraints)),
	  local_video_track_(nullptr),
	  remote_video_track_(nullptr),
	  audio_track_(nullptr),
	  events_cb_(nullptr),
	  ice_candidate_cb_(nullptr),
	  id_(id),
	  media_track_update_cb_(nullptr)
{
	pc_->RegisterRTCPeerConnectionObserver(this);
}

RTCClient::~RTCClient()
{
	pcf_ = nullptr;
	pc_ = nullptr;
	events_cb_ = nullptr;
	ice_candidate_cb_ = nullptr;
	local_video_track_ = nullptr;
	remote_video_track_ = nullptr;
	media_track_update_cb_ = nullptr;
}

std::string RTCClient::ID() const
{
	return id_;
}

void RTCClient::Close()
{
	// unregister pc observer
	pc_->DeRegisterRTCPeerConnectionObserver();

	// remove tracks
	if (local_video_track_) {
		auto streams = pc_->local_streams();
		for (int i = 0; i < streams.size(); i++) {
			auto stream = streams[i];
			stream->RemoveTrack(local_video_track_);
		}
	}
	if (remote_video_track_) {
		auto streams = pc_->remote_streams();
		for (int i = 0; i < streams.size(); i++) {
			auto stream = streams[i];
			stream->RemoveTrack(remote_video_track_);
		}
	}

	// delete peerconnection
	if (g_pcf_) {
		g_pcf_->Delete(pc_);
	}
}

void RTCClient::AddPeerconnectionEventsObserver(RTCClientConnectionObserver *cb)
{
	events_cb_ = cb;
}

void RTCClient::AddIceCandidateObserver(RTCClientIceCandidateObserver *cb)
{
	ice_candidate_cb_ = cb;
}

void RTCClient::AddMediaTrackUpdateObserver(RTCClientMediaTrackEventObserver *cb)
{
	media_track_update_cb_ = cb;
}

void RTCClient::CreateOffer(void *params, OnCreatedSdpCallback callback)
{
	scoped_refptr<RTCMediaConstraints> constraints =
		RTCMediaConstraints::Create();
	pc_->CreateOffer(
		[=](const string sdp, const string type) {
			if (callback) {
				std::string empty("");
				rtc::RTCSessionDescription s = {
					sdp.std_string(), type.std_string()};
				callback(s, empty, params);
			}
		},
		[=](const char *erro) {
			std::string e(erro);
			if (callback) {
				rtc::RTCSessionDescription s;
				callback(s, e, params);
			}
			blog(LOG_ERROR, "CreateOffer failed: %s", erro);
		},
		constraints);
}

void RTCClient::CreateAnswer(void *params, OnCreatedSdpCallback callback)
{
	scoped_refptr<RTCMediaConstraints> constraints =
		RTCMediaConstraints::Create();
	pc_->CreateAnswer(
		[=](const string sdp, const string type) {
			if (callback) {
				rtc::RTCSessionDescription s = {
					sdp.std_string(), type.std_string()};
				std::string empty("");
				callback(s, empty, params);
			}
		},
		[=](const char *erro) {
			std::string e(erro);
			rtc::RTCSessionDescription s;
			if (callback) {
				callback(s, e, params);
			}
			blog(LOG_ERROR, "CreateAnswer failed: %s", erro);
		},
		constraints);
}

void RTCClient::SetLocalDescription(const char *sdp, const char *type,
				    void *params, ErrorCallback callback)
{
	pc_->SetLocalDescription(
		string(sdp), string(type),
		[=]() {
			if (callback) {
				std::string empty("");
				callback(empty, params);
			}
		},
		[=](const char *erro) {
			std::string e(erro);
			if (callback) {
				callback(e, params);
			}
			blog(LOG_ERROR, "SetLocalDescription failed: %s", erro);
		});
}

void RTCClient::SetRemoteDescription(const char *sdp, const char *type,
				     void *params, ErrorCallback callback)
{
	pc_->SetRemoteDescription(
		string(sdp), string(type),
		[=]() {
			if (callback) {
				std::string empty("");
				callback(empty, params);
			}
		},
		[=](const char *erro) {
			std::string e(erro);
			if (callback) {
				callback(e, params);
			}
			blog(LOG_ERROR, "SetRemoteDescription failed: %s",
			     erro);
		});
}

void RTCClient::GetLocalDescription(void *params, OnCreatedSdpCallback callback)
{
	pc_->GetLocalDescription(
		[=](const string sdp, const string type) {
			if (callback) {
				rtc::RTCSessionDescription s = {
					sdp.std_string(), type.std_string()};
				std::string empty("");
				callback(s, empty, params);
			}
		},
		[=](const char *erro) {
			std::string e(erro);
			if (callback) {
				rtc::RTCSessionDescription s;
				callback(s, e, params);
			}
			blog(LOG_ERROR, "GetLocalDescription failed: %s", erro);
		});
}

void RTCClient::GetRemoteDescription(void *params,
				     OnCreatedSdpCallback callback)
{
	pc_->GetRemoteDescription(
		[=](const string sdp, const string type) {
			if (callback) {
				rtc::RTCSessionDescription s = {
					sdp.std_string(), type.std_string()};
				std::string empty("");
				callback(s, empty, params);
			}
		},
		[=](const char *erro) {
			std::string e(erro);
			if (callback) {
				rtc::RTCSessionDescription s;
				callback(s, e, params);
			}

			blog(LOG_ERROR, "GetRemoteDescription failed: %s",
			     erro);
		});
}

// Candiate
void RTCClient::AddCandidate(const char *mid, int mid_mline_index,
			     const char *candidate)
{
	pc_->AddCandidate(string(mid), mid_mline_index, string(candidate));
}

bool RTCClient::ToggleMute(bool mute)
{
	if (audio_track_ == nullptr)
		return false;

	return audio_track_->set_enabled(!mute);
}

void RTCClient::OnSignalingState(RTCSignalingState state)
{
	blog(LOG_INFO, "OnSignalingState: %d", state);
	if (events_cb_ != nullptr) {
		events_cb_->OnSignalingState(id_, state);
	}
}

void RTCClient::OnPeerConnectionState(RTCPeerConnectionState state)
{
	blog(LOG_INFO, "OnPeerConnectionState: %d", state);
	if (events_cb_ != nullptr) {
		events_cb_->OnPeerConnectionState(id_, state);
	}
}

void RTCClient::OnIceGatheringState(RTCIceGatheringState state)
{
	blog(LOG_INFO, "OnIceGatheringState: %d", state);
	if (events_cb_ != nullptr) {
		events_cb_->OnIceGatheringState(id_, state);
	}
}

void RTCClient::OnIceConnectionState(RTCIceConnectionState state)
{
	blog(LOG_INFO, "OnIceConnectionState: %d", state);
	if (events_cb_ != nullptr) {
		events_cb_->OnIceConnectionState(id_, state);
	}
}

void RTCClient::OnIceCandidate(
	scoped_refptr<libwebrtc::RTCIceCandidate> candidate)
{
	if (ice_candidate_cb_ != nullptr) {
		RTCIceCandidate candidate_ = {
			candidate->candidate().std_string(),
			candidate->sdp_mline_index(),
			candidate->sdp_mid().std_string()};
		ice_candidate_cb_->OnIceCandidateDiscoveried(id_, candidate_);
	}
}

void RTCClient::OnAddStream(scoped_refptr<RTCMediaStream> stream)
{
	if (stream->video_tracks().size() > 0) {
		remote_video_track_ = stream->video_tracks()[0];
	}
	if (stream->audio_tracks().size() > 0) {
		audio_track_ = stream->audio_tracks()[0];
	}
	if (media_track_update_cb_ != nullptr) {
		media_track_update_cb_->OnMediaTrackChanged(id_, kVideo,
							    kAdded);
	}
}

void RTCClient::OnRemoveStream(scoped_refptr<RTCMediaStream> stream) {}

void RTCClient::OnDataChannel(scoped_refptr<RTCDataChannel> data_channel) {}

void RTCClient::OnRenegotiationNeeded() {}

void RTCClient::OnTrack(scoped_refptr<RTCRtpTransceiver> transceiver) {}

void RTCClient::OnAddTrack(vector<scoped_refptr<RTCMediaStream>> streams,
			   scoped_refptr<RTCRtpReceiver> receiver)
{
}

void RTCClient::OnRemoveTrack(scoped_refptr<RTCRtpReceiver> receiver) {}

/////////////////////////////////////////////////////////////////////////////

void UpdateRTCLogLevel(janus::rtc::RTCLogLevel level)
{
	LibWebRTC::UpdateRTCLogLevel(level);
}

void IntializationPeerConnectionFactory()
{
	g_pcf_ = nullptr;
	LibWebRTC::Initialize();
	g_pcf_ = LibWebRTC::CreateRTCPeerConnectionFactory();
}

void ResetPeerConnectionFactorySettings()
{
	if (g_pcf_) {
		g_pcf_->Terminate();
		g_pcf_ = nullptr;
	}
}

void SetVideoHardwareAccelerationEnabled(bool enable)
{
	if (GlobalConfiguration::GetVideoHardwareAccelerationEnabled() ==
	    enable)
		return;
	ResetPeerConnectionFactorySettings();
	GlobalConfiguration::SetVideoHardwareAccelerationEnabled(enable);
}

RTCClient *CreateClient(
	std::vector<std::unordered_map<std::string, std::string>> &iceServers,
	std::string id)
{
	if (g_pcf_ == nullptr) {
		// Default log level is none
		UpdateRTCLogLevel(kNone);
		SetVideoHardwareAccelerationEnabled(true);
		IntializationPeerConnectionFactory();
	}

	const size_t l = iceServers.size();
	if (l == 0) {
		return nullptr;
	}

	RTCConfiguration configs;
	for (int i = 0; i < l; i++) {
		auto &t = iceServers[i];
		IceServer server = {t["uri"], t["username"], t["passwd"]};
		configs.ice_servers[i] = server;
	}

	scoped_refptr<RTCMediaConstraints> constraints =
		RTCMediaConstraints::Create();
	return new RTCClient(id, g_pcf_, configs, constraints);
}

/////////////////////////////////////////////////////////////////////////////

} // namespace janus::rtc
