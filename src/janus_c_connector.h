#ifndef JANUS_CONNECTION_API_H
#define JANUS_CONNECTION_API_H

#ifdef __cplusplus
extern "C" {
#endif

void *CreateConncetion();
void DestoryConnection(void *conn);

#ifdef __cplusplus
}
#endif

#endif // !JANUS_CONNECTION_API_H
