#pragma once

#include "libwebrtc.h"
#include "rtc_peerconnection.h"

using namespace libwebrtc;

namespace libwebrtc {
class RTCPeerConnectionFactory;
class RTCVideoCapturer;
}  // namespace libwebrtc

typedef void(__stdcall* StringCallback)(const char* content, const char* error);
typedef void(__stdcall* ErrorCallback)(const char* error);
typedef void(__stdcall* EventEnumCallback)(int type, int value);
typedef void(__stdcall* IceCandidateCallback)(const char* sdp,
                                              const char* mid,
                                              int mline_index);
typedef void(__stdcall* MediaTrackUpdateCallback)(int type, int state);

class RTCClient : public RTCPeerConnectionObserver {
 public:
  enum ClientEventsType {
    SignalingState = 0,
    PeerConnectionState,
    IceGatheringState,
    IceConnectionState,
  };

  RTCClient(scoped_refptr<RTCPeerConnectionFactory> pcf,
            const RTCConfiguration& configuration,
            scoped_refptr<RTCMediaConstraints> constraints);
  ~RTCClient();

  scoped_refptr<RTCVideoTrack> local_video_track_;
  scoped_refptr<RTCAudioTrack> audio_track_;
  scoped_refptr<RTCVideoTrack> remote_video_track_;

  void Close();

  // SDP
  void CreateOffer(StringCallback sdp);
  void CreateAnswer(StringCallback sdp);
  void SetLocalDescription(const char* sdp, const char* type, ErrorCallback cb);
  void SetRemoteDescription(const char* sdp,
                            const char* type,
                            ErrorCallback cb);
  void GetLocalDescription(StringCallback cb);
  void GetRemoteDescription(StringCallback cb);

  // Candidate
  void AddCandidate(const char* mid,
                    int mid_mline_index,
                    const char* candidate);

  // Media
  bool ToggleMute(bool mute);

  // Renderer
  void AttachVideoRenderer(
      RTCVideoRenderer<scoped_refptr<RTCVideoFrame>>* renderer,
      bool local);
  void DetachVideoRenderer(
      RTCVideoRenderer<scoped_refptr<RTCVideoFrame>>* renderer,
      bool local);

  // PC Observer & callback
  void AddPeerconnectionEventsObserver(EventEnumCallback cb);
  void AddIceCandidateObserver(IceCandidateCallback cb);
  void AddMediaTrackUpdateObserver(MediaTrackUpdateCallback cb);

  virtual void OnSignalingState(RTCSignalingState state) override;
  virtual void OnPeerConnectionState(RTCPeerConnectionState state) override;
  virtual void OnIceGatheringState(RTCIceGatheringState state) override;
  virtual void OnIceConnectionState(RTCIceConnectionState state) override;
  virtual void OnIceCandidate(
      scoped_refptr<RTCIceCandidate> candidate) override;
  virtual void OnAddStream(scoped_refptr<RTCMediaStream> stream) override;
  virtual void OnRemoveStream(scoped_refptr<RTCMediaStream> stream) override;
  virtual void OnDataChannel(
      scoped_refptr<RTCDataChannel> data_channel) override;
  virtual void OnRenegotiationNeeded() override;
  virtual void OnTrack(scoped_refptr<RTCRtpTransceiver> transceiver) override;
  virtual void OnAddTrack(vector<scoped_refptr<RTCMediaStream>> streams,
                          scoped_refptr<RTCRtpReceiver> receiver) override;
  virtual void OnRemoveTrack(scoped_refptr<RTCRtpReceiver> receiver) override;

 private:
  scoped_refptr<RTCPeerConnectionFactory> pcf_;
  scoped_refptr<RTCPeerConnection> pc_;
  std::string selected_mic_name_ = "";

  EventEnumCallback events_cb_;
  IceCandidateCallback ice_candidate_cb_;
  MediaTrackUpdateCallback media_track_update_cb_;
};

