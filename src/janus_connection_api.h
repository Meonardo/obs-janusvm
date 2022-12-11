#ifndef JANUS_CONNECTION_API_H
#define JANUS_CONNECTION_API_H

#ifdef __cplusplus
extern "C" {
#endif

/// <summary>
/// Create the `JanusConnection` instance
/// </summary>
/// <returns>the `JanusConnection` instance ptr</returns>
void *CreateConncetion();
/// <summary>
/// Destory the `JanusConnection` instance
/// </summary>
/// <param name="conn">the `JanusConnection` instance ptr</param>
void DestoryConnection(void *conn);

/// <summary>
/// Publish media stream to janus video-room plugin
/// </summary>
/// <param name="conn">the `JanusConnection` instance ptr</param>
/// <param name="url">the janus server(ws) address</param>
/// <param name="id">the user id</param>
/// <param name="display">the user's display name in the room</param>
/// <param name="room">the room this user will join</param>
/// <param name="pin">the passwd of this room</param>
void Publish(void *conn, const char *url, uint32_t id, const char *display,
	     uint64_t room, const char *pin);

/// <summary>
/// Cancel publish media stream to janus video-room plugin
/// </summary>
/// <param name="conn"></param>
void Unpublish(void *conn);

/// <summary>
/// Register audio/video provider for janus 
/// </summary>
/// <param name="conn">the `JanusConnection` instance ptr</param>
/// <param name="provider">the `janus_output` pointer</param>
void RegisterVideoProvider(void *conn, void *provider);

#ifdef __cplusplus
}
#endif

#endif // !JANUS_CONNECTION_API_H
