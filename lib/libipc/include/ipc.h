#ifndef IPC_H
#define IPC_H

/* libipc — PhoenixOS IPC (message-passing) library */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t u32;
typedef uint8_t  u8;
typedef int32_t  s32;

/* ── Message ──────────────────────────────────────────────────────────────── */

#define IPC_DATA_MAX  256

typedef enum {
    MSG_NONE       = 0,
    MSG_PING       = 1,
    MSG_PONG       = 2,
    MSG_CONNECT    = 10,
    MSG_DISCONNECT = 11,
    MSG_DATA       = 20,
    /* GUI messages */
    MSG_WIN_CREATE   = 100,
    MSG_WIN_DESTROY  = 101,
    MSG_WIN_SHOW     = 102,
    MSG_WIN_HIDE     = 103,
    MSG_WIN_MOVE     = 104,
    MSG_WIN_RESIZE   = 105,
    MSG_WIN_RAISE    = 106,
    MSG_WIN_PAINT    = 107,
    MSG_WIN_EVENT    = 108,
    /* Service control */
    MSG_SVC_START    = 200,
    MSG_SVC_STOP     = 201,
    MSG_SVC_STATUS   = 202,
    MSG_SVC_RESTART  = 203,
} MessageType;

typedef struct {
    u32          type;
    u32          sender;      /* PID of sender */
    u32          recipient;   /* PID of recipient, 0=broadcast */
    u32          seq;         /* sequence number */
    u32          len;         /* bytes used in data[] */
    u8           data[IPC_DATA_MAX];
} Message;

/* ── Channel (connection endpoint) ───────────────────────────────────────── */

typedef struct {
    int      fd;          /* underlying socket/pipe fd */
    u32      peer_pid;
    u32      local_pid;
    char     name[64];
} Channel;

/* ── API ──────────────────────────────────────────────────────────────────── */

/* Server side */
int  ipc_listen(const char *name);        /* bind to a named socket */
int  ipc_accept(int listen_fd, Channel *ch); /* accept incoming connection */

/* Client side */
int  ipc_connect(const char *name, Channel *ch);  /* connect by name */

/* Both sides */
int  ipc_send(Channel *ch, Message *msg);
int  ipc_recv(Channel *ch, Message *msg);
int  ipc_recv_timeout(Channel *ch, Message *msg, int ms);

void ipc_close(Channel *ch);

/* Convenience helpers */
int  ipc_call(Channel *ch, u32 type, const void *data, u32 len, Message *reply);
int  ipc_broadcast(const char *socket_dir, Message *msg);

#ifdef __cplusplus
}
#endif

#endif /* IPC_H */
