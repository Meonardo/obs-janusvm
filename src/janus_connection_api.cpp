#include "janus_connection.h"

#ifdef __cplusplus
extern "C" {
#endif

void *CreateConncetion(bool encoded)
{
	return new janus::JanusConnection(encoded);
}

void DestoryConnection(void *conn)
{
	auto janus_conn = static_cast<janus::JanusConnection *>(conn);
	if (janus_conn != nullptr) {
		delete conn;
		conn = nullptr;
	}
}

void Publish(void *conn, const char *url, uint32_t id, const char *display,
	     uint64_t room,
	     const char *pin)
{
	auto janus_conn = static_cast<janus::JanusConnection *>(conn);
	janus_conn->Publish(url, id, display, room, pin);
}

void Unpublish(void *conn)
{
	auto janus_conn = static_cast<janus::JanusConnection *>(conn);
	janus_conn->Unpublish();
}

void SendVideoFrame(void *conn, void *video_frame, int width, int height)
{
	auto janus_conn = reinterpret_cast<janus::JanusConnection *>(conn);
	auto frame = reinterpret_cast<OBSVideoFrame *>(video_frame);
	janus_conn->SendVideoFrame(frame, width, height);
}

void SendVideoPacket(void *conn, void *packet, int width, int height)
{
	auto janus_conn = reinterpret_cast<janus::JanusConnection *>(conn);
	auto pkt = reinterpret_cast<OBSVideoPacket *>(packet);
	janus_conn->SendVideoPacket(pkt, width, height);
}

void SendAudioFrame(void *conn, void *audio_frame)
{
	auto janus_conn = reinterpret_cast<janus::JanusConnection *>(conn);
	auto frame = reinterpret_cast<OBSAudioFrame *>(audio_frame);
	janus_conn->SendAudioFrame(frame);
}

#ifdef __cplusplus
}
#endif
