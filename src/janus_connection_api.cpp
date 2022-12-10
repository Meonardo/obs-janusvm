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

#ifdef __cplusplus
}
#endif
