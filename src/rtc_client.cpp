#include "rtc_client.h"
#include <iostream>
#include "rtc_audio_device.h"
#include "rtc_audio_source.h"
#include "rtc_desktop_device.h"
#include "rtc_peerconnection_factory.h"

using namespace libwebrtc;

static scoped_refptr<RTCPeerConnectionFactory> g_pcf_;

RTCClient::RTCClient(scoped_refptr<RTCPeerConnectionFactory> pcf,
                     const RTCConfiguration& configuration,
                     scoped_refptr<RTCMediaConstraints> constraints)
    : pcf_(pcf),
      pc_(pcf->Create(configuration, constraints)),
      local_video_track_(nullptr),
      remote_video_track_(nullptr),
      audio_track_(nullptr),
      events_cb_(nullptr),
      ice_candidate_cb_(nullptr),
      media_track_update_cb_(nullptr) {
  pc_->RegisterRTCPeerConnectionObserver(this);
}

RTCClient::~RTCClient() {
  pc_ = nullptr;
  events_cb_ = nullptr;
  ice_candidate_cb_ = nullptr;
  local_video_track_ = nullptr;
  remote_video_track_ = nullptr;
  media_track_update_cb_ = nullptr;
}

void RTCClient::Close() {
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

void RTCClient::AddPeerconnectionEventsObserver(EventEnumCallback cb) {
  events_cb_ = cb;
}

void RTCClient::AddIceCandidateObserver(IceCandidateCallback cb) {
  ice_candidate_cb_ = cb;
}

void RTCClient::CreateOffer(StringCallback callback) {
  scoped_refptr<RTCMediaConstraints> constraints =
      RTCMediaConstraints::Create();
  pc_->CreateOffer(
      [callback](const string sdp, const string type) {
        if (callback) {
          callback(sdp.c_string(), "");
        }
      },
      [callback](const char* erro) {
        std::string e(erro);
        if (callback) {
          callback("", erro);
        }
        std::cout << "------------ Offer sdp failed: " << e << std::endl;
      },
      constraints);
}

void RTCClient::CreateAnswer(StringCallback callback) {
  scoped_refptr<RTCMediaConstraints> constraints =
      RTCMediaConstraints::Create();
  pc_->CreateAnswer(
      [callback](const string sdp, const string type) {
        if (callback) {
          callback(sdp.c_string(), "");
        }
      },
      [callback](const char* erro) {
        std::string e(erro);
        if (callback) {
          callback("", erro);
        }
        std::cout << "------------ Answer sdp failed: " << e << std::endl;
      },
      constraints);
}

void RTCClient::SetLocalDescription(const char* sdp,
                                    const char* type,
                                    ErrorCallback callback) {
  pc_->SetLocalDescription(
      string(sdp), string(type),
      [callback]() {
        if (callback) {
          callback("");
        }
      },
      [callback](const char* erro) {
        std::string e(erro);
        if (callback) {
          callback(erro);
        }
        std::cout << "------------ Set local sdp failed: " << e << std::endl;
      });
}

void RTCClient::SetRemoteDescription(const char* sdp,
                                     const char* type,
                                     ErrorCallback callback) {
  pc_->SetRemoteDescription(
      string(sdp), string(type),
      [callback]() {
        if (callback) {
          callback("");
        }
      },
      [callback](const char* erro) {
        std::string e(erro);
        if (callback) {
          callback(erro);
        }
        std::cout << "------------ Set remote sdp failed: " << e << std::endl;
      });
}

void RTCClient::GetLocalDescription(StringCallback callback) {
  pc_->GetLocalDescription(
      [callback](const string sdp, const string type) {
        if (callback) {
          callback(sdp.c_string(), "");
        }
      },
      [callback](const char* erro) {
        std::string e(erro);
        if (callback) {
          callback("", erro);
        }
        std::cout << "------------ Get local sdp failed: " << e << std::endl;
      });
}

void RTCClient::GetRemoteDescription(StringCallback callback) {
  pc_->GetRemoteDescription(
      [callback](const string sdp, const string type) {
        if (callback) {
          callback(sdp.c_string(), "");
        }
      },
      [callback](const char* erro) {
        std::string e(erro);
        if (callback) {
          callback("", erro);
        }
        std::cout << "------------ Get local sdp failed: " << e << std::endl;
      });
}

// Candiate
void RTCClient::AddCandidate(const char* mid,
                             int mid_mline_index,
                             const char* candidate) {
  pc_->AddCandidate(string(mid), mid_mline_index, string(candidate));
}

bool RTCClient::ToggleMute(bool mute) {
  if (audio_track_ == nullptr)
    return false;

  return audio_track_->set_enabled(!mute);
}

void RTCClient::AttachVideoRenderer(
    RTCVideoRenderer<scoped_refptr<RTCVideoFrame>>* renderer,
    bool local) {
  if (local) {
    if (local_video_track_ != nullptr) {
      local_video_track_->AddRenderer(renderer);
    }
  } else {
    if (remote_video_track_ != nullptr) {
      remote_video_track_->AddRenderer(renderer);
    }
  }
}

void RTCClient::DetachVideoRenderer(
    RTCVideoRenderer<scoped_refptr<RTCVideoFrame>>* renderer,
    bool local) {
  if (local) {
    if (local_video_track_ != nullptr && renderer != nullptr) {
      local_video_track_->RemoveRenderer(renderer);
    }
  } else {
    if (remote_video_track_ != nullptr) {
      remote_video_track_->RemoveRenderer(renderer);
    }
  }

  // !!! At this point it will delete the VideoRenderer !!!
  delete renderer;
}

void RTCClient::AddMediaTrackUpdateObserver(MediaTrackUpdateCallback cb) {
  media_track_update_cb_ = cb;
}

void RTCClient::OnSignalingState(RTCSignalingState state) {
  if (events_cb_ != nullptr) {
    events_cb_(SignalingState, state);
  }
}

void RTCClient::OnPeerConnectionState(RTCPeerConnectionState state) {
  if (events_cb_ != nullptr) {
    events_cb_(PeerConnectionState, state);
  }
}

void RTCClient::OnIceGatheringState(RTCIceGatheringState state) {
  if (events_cb_ != nullptr) {
    events_cb_(IceGatheringState, state);
  }
}

void RTCClient::OnIceConnectionState(RTCIceConnectionState state) {
  if (events_cb_ != nullptr) {
    events_cb_(IceConnectionState, state);
  }
}

void RTCClient::OnIceCandidate(scoped_refptr<RTCIceCandidate> candidate) {
  if (ice_candidate_cb_ != nullptr) {
    ice_candidate_cb_(candidate->candidate().c_string(),
                      candidate->sdp_mid().c_string(),
                      candidate->sdp_mline_index());
  }
}

void RTCClient::OnAddStream(scoped_refptr<RTCMediaStream> stream) {
  if (stream->video_tracks().size() > 0) {
    remote_video_track_ = stream->video_tracks()[0];
  }
  if (stream->audio_tracks().size() > 0) {
    audio_track_ = stream->audio_tracks()[0];
  }
  if (media_track_update_cb_ != nullptr) {
    media_track_update_cb_(1, 1);
  }
}

void RTCClient::OnRemoveStream(scoped_refptr<RTCMediaStream> stream) {}

void RTCClient::OnDataChannel(scoped_refptr<RTCDataChannel> data_channel) {}

void RTCClient::OnRenegotiationNeeded() {}

void RTCClient::OnTrack(scoped_refptr<RTCRtpTransceiver> transceiver) {}

void RTCClient::OnAddTrack(vector<scoped_refptr<RTCMediaStream>> streams,
                           scoped_refptr<RTCRtpReceiver> receiver) {}

void RTCClient::OnRemoveTrack(scoped_refptr<RTCRtpReceiver> receiver) {}
