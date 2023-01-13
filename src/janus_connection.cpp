#include "janus_connection.h"
#include "nlohmann/json.hpp"

namespace janus {

VideoFeederImpl::VideoFeederImpl()
	: frame_receiver_(nullptr), packet_receiver_(nullptr)
{
}

VideoFeederImpl::~VideoFeederImpl()
{
	frame_receiver_ = nullptr;
}

void VideoFeederImpl::FeedVideoFrame(OBSVideoFrame *frame, int width,
					  int height)
{
	auto v_frame = libwebrtc::RTCVideoFrame::Create(
		width, height, frame->data[0], frame->data[1]);
	if (frame_receiver_ != nullptr) {
		frame_receiver_->OnFrame(v_frame);
	}
}

void VideoFeederImpl::SetFrameReceiver(
	owt::base::VideoFrameReceiverInterface *receiver)
{
	frame_receiver_ = receiver;
}

void VideoFeederImpl::SetBufferReceiver(
	owt::base::VideoPacketReceiverInterface *receiver)
{
	packet_receiver_ = receiver;
}

void VideoFeederImpl::FeedVideoPacket(OBSVideoPacket *pkt, int width,
					 int height)
{
	auto encoded_frame = libwebrtc::RTCVideoFrame::Create(pkt->data, pkt->size, pkt->keyframe,
		width, height);
	if (packet_receiver_ != nullptr) {
		packet_receiver_->OnPacket(encoded_frame);
	}
}

AudioFeederImpl::AudioFeederImpl() : frame_receiver_(nullptr)
{
	// get audio info from obs output
	auto audio = obs_get_audio();
	auto info = audio_output_get_info(audio);
	channels_ = audio_output_get_channels(audio);
	sample_rate_ = audio_output_get_sample_rate(audio);
	audio_bytes_per_channel_ = get_audio_bytes_per_channel(info->format);
}

AudioFeederImpl::~AudioFeederImpl()
{
	frame_receiver_ = nullptr;
}

int AudioFeederImpl::GetSampleRate()
{
	return sample_rate_;
}

int AudioFeederImpl::GetChannelNumber()
{
	return channels_;
}

void AudioFeederImpl::SetAudioFrameReceiver(
	owt::base::AudioFrameReceiverInterface *receiver)
{
	frame_receiver_ = receiver;
}

void AudioFeederImpl::FeedAudioFrame(OBSAudioFrame *frame) {
	if (frame_receiver_ == nullptr)
		return;

	frame_receiver_->OnFrame(frame->data[0], frame->frames);
}

/////////////////////////////////////////////////////////////////////////////////

JanusConnection::JanusConnection(bool send_encoded_data)
	: ws_client_(nullptr),
	  rtc_client_(nullptr),
	  video_feeder_(nullptr),
	  room_(0),
	  session_id_(0),
	  handle_id_(0),
	  id_(0),
	  joined_room_(false),
	  use_encoded_data_(send_encoded_data)
{
	// create customized audio input
	audio_feeder_ = std::make_shared<AudioFeederImpl>();
	rtc::SetCustomizedAudioInputEnabled(true, audio_feeder_);
}

JanusConnection::~JanusConnection()
{
	// destory customized audio input
	rtc::SetCustomizedAudioInputEnabled(false, nullptr);
	audio_feeder_ = nullptr;

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

void JanusConnection::OnConnected()
{
	// create janus sesssion
	CreateSession();
}

void JanusConnection::OnConnectionClosed(const std::string &reason)
{
	joined_room_ = false;
	InvalidKeepaliveThread();
}

void JanusConnection::OnRecvMessage(const std::string &msg)
{
	auto json = nlohmann::json::parse(msg);
	if (json.contains("transaction") && json.contains("janus")) {
		std::string transaction = json["transaction"];
		std::string janus = json["janus"];

		if (transaction == "Create" && janus == "success") {
			uint64_t session_id = json["data"]["id"];
			session_id_ = session_id;
			// get handle ID
			CreateHandle();
			// send keep-alive msg in every 20s
			CreateKeepaliveThread();
		} else if (transaction == "Attach" && janus == "success") {
			uint64_t hdl_id = json["data"]["id"];
			handle_id_ = hdl_id;
			// publish media stream automatically
			Publish(nullptr, id_, display_.c_str(), room_,
				pin_.c_str());
		} else if (transaction == "JoinRoom" && janus == "event") {
			// joined the room
			CreateRTCClient();
			CreateOffer();
			// set join room state to `true`
			joined_room_ = true;
		} else if (transaction == "Configure" && janus == "event") {
			// process configs & set remote offer
			std::string sdp = json["jsep"]["sdp"];
			SetAnswer(sdp);
		}
	} else if (json.contains("janus")) {
		std::string janus = json["janus"];
		if (janus == "hangup") {
			// hangup from janus
		}
	}
}

void JanusConnection::OnIceCandidateDiscoveried(std::string &id, rtc::RTCIceCandidate &candidate)
{
	SendCandidate(candidate.sdp, candidate.sdp_mid, candidate.sdp_mline_index);
}

void JanusConnection::Publish(const char *url, uint32_t id, const char *display,
			      uint64_t room, const char *pin)
{
	id_ = id;
	display_ = display;
	room_ = room;
	pin_ = pin;

	if (ws_client_ == nullptr || !ws_client_->Connected()) {
		// make a connection first
		Connect(url);
	} else {
		if (!joined_room_) {
			nlohmann::json payload = {{"janus", "message"},
						  {"transaction", "JoinRoom"},
						  {"handle_id", handle_id_},
						  {"session_id", session_id_},
						  {"body",
						   {{"request", "join"},
						    {"ptype", "publisher"},
						    {"room", room},
						    {"pin", pin},
						    {"display", display},
						    {"id", id}}}};
			std::string msg = payload.dump();
			ws_client_->SendMsg(msg);
		} else {
			CreateRTCClient();
			CreateOffer();
		}
	}
}

void JanusConnection::Unpublish()
{
	nlohmann::json payload = {{"janus", "message"},
				  {"transaction", "Unpublish"},
				  {"handle_id", handle_id_},
				  {"session_id", session_id_},
				  {"body", {{"request", "unpublish"}}}};
	std::string msg = payload.dump();
	ws_client_->SendMsg(msg);

	// destory RTCClient
	DestoryRTCClient();
	// release the `VideoFrameFeeder`
	if (video_feeder_) {
		delete video_feeder_;
		video_feeder_ = nullptr;
	}
}

void JanusConnection::CreateRTCClient()
{
	if (rtc_client_ != nullptr)
		return;

	typedef std::unordered_map<std::string, std::string> ICEServer;
	auto ice_servers = std::vector<ICEServer>{};
	// default ice servers
	{
		ICEServer map = {
			{"uri", "stun:120.79.19.54:3478"},
			{"username", "amdox"},
			{"passwd", "123456"},
		};
		ice_servers.push_back(map);
	}
	{
		ICEServer map = {
			{"uri", "turn:120.79.19.54:3478"},
			{"username", "amdox"},
			{"passwd", "123456"},
		};
		ice_servers.push_back(map);
	}
	std::string id("obs");
	rtc_client_ = rtc::CreateClient(ice_servers, id);
}

rtc::RTCClient *JanusConnection::GetRTCClient() const
{
	return rtc_client_;
}

void JanusConnection::SendVideoFrame(OBSVideoFrame *frame, int width,
				     int height)
{
	if (video_feeder_ == nullptr)
		return;
	video_feeder_->FeedVideoFrame(frame, width, height);
}

void JanusConnection::SendVideoPacket(OBSVideoPacket *pkt, int width,
				      int height)
{
	if (video_feeder_ == nullptr)
		return;
	video_feeder_->FeedVideoPacket(pkt, width, height);
}

void JanusConnection::SendAudioFrame(OBSAudioFrame *frame) {
	if (audio_feeder_ == nullptr) {
		return;
	}
	audio_feeder_->FeedAudioFrame(frame);
}

void JanusConnection::DestoryRTCClient()
{
	if (rtc_client_ == nullptr)
		return;

	rtc_client_->Close();
	delete rtc_client_;
	rtc_client_ = nullptr;
}

void JanusConnection::CreateSession()
{
	nlohmann::json payload = {{"janus", "create"},
				  {"transaction", "Create"}};
	std::string create_msg = payload.dump();
	ws_client_->SendMsg(create_msg);
}

void JanusConnection::CreateHandle()
{
	nlohmann::json payload = {{"janus", "attach"},
				  {"transaction", "Attach"},
				  {"plugin", "janus.plugin.videoroom"},
				  {"session_id", session_id_}};
	std::string msg = payload.dump();
	ws_client_->SendMsg(msg);
}

void JanusConnection::CreateOffer()
{
	// create video framer if necessary
	if (video_feeder_ == nullptr) {
		video_feeder_ = new VideoFeederImpl();
	}

	if (rtc_client_ == nullptr)
		return;

	// create media sender
	if (use_encoded_data_) {
		rtc_client_->CreateMediaSender(video_feeder_, true);
	} else {
		rtc_client_->CreateMediaSender(video_feeder_);
	}

	// create offer
	rtc_client_->CreateOffer(this, [](janus::rtc::RTCSessionDescription &sdp,
					  std::string &error, void *params) {
		auto self = reinterpret_cast<janus::JanusConnection *>(params);
		if (self != nullptr && error.empty()) {
			// set local sdp
			self->GetRTCClient()->SetLocalDescription(
				sdp.sdp.c_str(), sdp.type.c_str(), NULL, NULL);
			// send to janus
			self->SendOffer(sdp.sdp);
		}
	});
}

void JanusConnection::SendOffer(std::string &sdp)
{
	nlohmann::json payload = {
		{"janus", "message"},
		{"transaction", "Configure"},
		{"handle_id", handle_id_},
		{"session_id", session_id_},
		{"body",
		 {{"request", "configure"}, {"audio", true}, {"video", true}}},
		{"jsep", {{"type", "offer"}, {"sdp", sdp}}}};
	std::string msg = payload.dump();
	ws_client_->SendMsg(msg);
}

void JanusConnection::SendCandidate(std::string &sdp, std::string &mid, int idx)
{
	nlohmann::json payload = {{"janus", "trickle"},
				  {"transaction", "Candidate"},
				  {"handle_id", handle_id_},
				  {"session_id", session_id_},
				  {"candidate",
				   {{"candidate", sdp},
				    {"sdpMid", mid},
				    {"sdpMLineIndex", idx}}}};
	std::string msg = payload.dump();
	ws_client_->SendMsg(msg);
}

void JanusConnection::SetAnswer(std::string &sdp)
{
	if (rtc_client_ == nullptr)
		return;

	rtc_client_->SetRemoteDescription(sdp.c_str(), "answer", NULL, NULL);
}

static void KeepAlive(uint64_t session, signaling::WebsocketClient *client)
{
	nlohmann::json payload = {{"janus", "keepalive"},
				  {"transaction", "Keepalive"},
				  {"session_id", session}};
	std::string msg = payload.dump();

	client->SendMsg(msg);
}

static void *StartSendingKeepalive(void *params)
{
	auto list = static_cast<std::list<void *> *>(params);
	auto ws_client =
		static_cast<signaling::WebsocketClient *>(list->front());
	auto session = static_cast<uint64_t *>(list->back());

	// set thread name
	os_set_thread_name("janus-keep-alive");

	while (*session > 0) {
		KeepAlive(*session, ws_client);
		Sleep(20 * 1000);
	}

	// this list created on the heap
	delete list;
	list = nullptr;

	return NULL;
}

int JanusConnection::CreateKeepaliveThread()
{
	auto list = new std::list<void *>{ws_client_, &session_id_};
	return pthread_create(&keeplive_thread_, NULL, StartSendingKeepalive,
			      list);
}

void JanusConnection::InvalidKeepaliveThread()
{
	// deal with the keep-alive thread & stop the loop
	session_id_ = 0;
	handle_id_ = 0;
	pthread_join(keeplive_thread_, NULL);
}

}
