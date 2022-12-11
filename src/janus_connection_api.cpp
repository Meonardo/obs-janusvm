#include "janus_connection.h"

#ifdef __cplusplus
extern "C" {
#endif

void *CreateConncetion()
{
	return new janus::JanusConnection();
}

void DestoryConnection(void *conn)
{
	auto ptr = static_cast<janus::JanusConnection *>(conn);
	if (ptr != nullptr) {
		delete conn;
		conn = nullptr;
	}
}

//void Connect(void* conn, const char* url)
//{
//	auto ptr = static_cast<janus::JanusConnection *>(conn);
//	if (ptr != nullptr) {
//		ptr->Connect(url);
//	}
//}
//
//void Disconnect(void* conn)
//{
//	auto ptr = static_cast<janus::JanusConnection *>(conn);
//	if (ptr != nullptr) {
//		ptr->Disconnect();
//	}
//}

void Publish(void *conn, const char *url, uint32_t id, const char *display,
	     uint64_t room,
	     const char *pin)
{
	auto ptr = static_cast<janus::JanusConnection *>(conn);
	if (ptr != nullptr) {
		ptr->Publish(url, id, display, room, pin);
	}
}

void Unpublish(void *conn)
{
	auto ptr = static_cast<janus::JanusConnection *>(conn);
	if (ptr != nullptr) {
		ptr->Unpublish();
	}
}

void RegisterVideoProvider(void *conn, void *media_provider)
{
	auto ptr = static_cast<janus::JanusConnection *>(conn);
	auto provider = static_cast<MediaProvider *>(media_provider);
	if (ptr != nullptr && provider != nullptr) {
		ptr->RegisterVideoProvider(provider);
	}
}

#ifdef __cplusplus
}
#endif
