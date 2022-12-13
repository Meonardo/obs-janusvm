#include "janus_connection.h"
#include "nlohmann/json.hpp"

namespace janus {

VideoFrameFeederImpl::VideoFrameFeederImpl()
	: frame_receiver_(nullptr)
{
}

VideoFrameFeederImpl::~VideoFrameFeederImpl() {}

void VideoFrameFeederImpl::FeedVideoFrame(OBSVideoFrame *frame, int width,
					  int height)
{
	auto v_frame = libwebrtc::RTCVideoFrame::Create(
		width, height, frame->data[0], frame->data[1]);
	if (frame_receiver_ != nullptr) {
		frame_receiver_->OnFrame(v_frame);
	}
}

void VideoFrameFeederImpl::SetFrameReceiver(
	owt::base::VideoFrameReceiverInterface *receiver)
{
	frame_receiver_ = receiver;
}

/////////////////////////////////////////////////////////////////////////////////

JanusConnection::JanusConnection()
	: ws_client_(nullptr),
	  rtc_client_(nullptr),
	  video_framer_(nullptr),
	  room_(0),
	  session_id_(0),
	  handle_id_(0),
	  id_(0)
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

void JanusConnection::OnConnected()
{
	// create janus sesssion
	CreateSession();
}

void JanusConnection::OnConnectionClosed(const std::string &reason) {}

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
		} else if (transaction == "Attach" && janus == "success") {
			uint64_t hdl_id = json["data"]["id"];
			handle_id_ = hdl_id;
			// send keep-alive msg in every 20s
			CreateKeepaliveThread();
			// publish media stream automatically
			Publish(nullptr, id_, display_.c_str(), room_,
				pin_.c_str());
		} else if (transaction == "JoinRoom" && janus == "event") {
			// joined the room
			CreateRTCClient();
			CreateOffer();
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

	// deal with the keep-alive thread & stop the loop
	session_id_ = 0;
	handle_id_ = 0;
	pthread_join(keeplive_thread_, NULL);
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
	rtc_client_ = rtc::CreateClient(ice_servers, "obs");
}

rtc::RTCClient *JanusConnection::GetRTCClient() const
{
	return rtc_client_;
}

void JanusConnection::SendVideoFrame(OBSVideoFrame *frame, int width,
				     int height)
{
	if (video_framer_ == nullptr) {
		video_framer_ = new VideoFrameFeederImpl();
	}
	video_framer_->FeedVideoFrame(frame, width, height);
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
	if (rtc_client_ == nullptr || video_framer_ == nullptr)
		return;

	// create media sender
	rtc_client_->CreateMediaSender(
		std::unique_ptr<VideoFrameFeederImpl>(video_framer_));
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

}
