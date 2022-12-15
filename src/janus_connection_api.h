#ifndef JANUS_CONNECTION_API_H
#define JANUS_CONNECTION_API_H

#ifdef __cplusplus
extern "C" {
#endif

/// <summary>
/// Create the `JanusConnection` instance
/// </summary>
/// <returns>the `JanusConnection` instance ptr</returns>
void *CreateConncetion(bool encoded);
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
/// Send video frame(NV12) to janus connetion
/// </summary>
/// <param name="conn">the `JanusConnection` instance ptr</param>
/// <param name="video_frame">video frame data in OBS</param>
/// <param name="width">video frame width</param>
/// <param name="height">video frame height</param>
void SendVideoFrame(void *conn, void *video_frame, int width, int height);


void SendVideoPacket(void *conn, void *packet, int width, int height);

#ifdef __cplusplus
}
#endif

#endif // !JANUS_CONNECTION_API_H
