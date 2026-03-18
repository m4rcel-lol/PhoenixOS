#include "include/ipc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <poll.h>

#define IPC_SOCKET_DIR  "/run/ipc"

/* ── ipc_listen ───────────────────────────────────────────────────────────── */

int ipc_listen(const char *name) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    /* Make socket non-blocking for accept loop */
    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    /* Build path directly into addr.sun_path (108 bytes on Linux) */
    snprintf(addr.sun_path, sizeof(addr.sun_path),
             "%s/%s.sock", IPC_SOCKET_DIR, name);

    unlink(addr.sun_path);  /* Remove stale socket */

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }
    if (listen(fd, 16) < 0) { close(fd); return -1; }
    return fd;
}

/* ── ipc_accept ───────────────────────────────────────────────────────────── */

int ipc_accept(int listen_fd, Channel *ch) {
    struct sockaddr_un peer;
    socklen_t plen = sizeof(peer);
    int fd = accept(listen_fd, (struct sockaddr *)&peer, &plen);
    if (fd < 0) return -1;

    memset(ch, 0, sizeof(*ch));
    ch->fd        = fd;
    ch->local_pid = (u32)getpid();
    return 0;
}

/* ── ipc_connect ──────────────────────────────────────────────────────────── */

int ipc_connect(const char *name, Channel *ch) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path),
             "%s/%s.sock", IPC_SOCKET_DIR, name);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd); return -1;
    }

    memset(ch, 0, sizeof(*ch));
    ch->fd        = fd;
    ch->local_pid = (u32)getpid();
    strncpy(ch->name, name, sizeof(ch->name) - 1);
    return 0;
}

/* ── ipc_send ─────────────────────────────────────────────────────────────── */

int ipc_send(Channel *ch, Message *msg) {
    msg->sender = ch->local_pid;
    ssize_t total = 0;
    ssize_t len   = (ssize_t)sizeof(Message);
    u8 *ptr = (u8 *)msg;
    while (total < len) {
        ssize_t n = write(ch->fd, ptr + total, (size_t)(len - total));
        if (n <= 0) return -1;
        total += n;
    }
    return 0;
}

/* ── ipc_recv_timeout ─────────────────────────────────────────────────────── */

int ipc_recv_timeout(Channel *ch, Message *msg, int ms) {
    struct pollfd pfd = { .fd = ch->fd, .events = POLLIN };
    int r = poll(&pfd, 1, ms < 0 ? -1 : ms);
    if (r <= 0) return r;  /* 0=timeout, -1=error */

    ssize_t total = 0;
    ssize_t len   = (ssize_t)sizeof(Message);
    u8 *ptr = (u8 *)msg;
    while (total < len) {
        ssize_t n = read(ch->fd, ptr + total, (size_t)(len - total));
        if (n <= 0) return -1;
        total += n;
    }
    return 1;
}

int ipc_recv(Channel *ch, Message *msg) {
    return ipc_recv_timeout(ch, msg, -1);
}

/* ── ipc_close ────────────────────────────────────────────────────────────── */

void ipc_close(Channel *ch) {
    if (ch->fd >= 0) { close(ch->fd); ch->fd = -1; }
}

/* ── ipc_call ─────────────────────────────────────────────────────────────── */

int ipc_call(Channel *ch, u32 type, const void *data, u32 len, Message *reply) {
    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    msg.len  = len < IPC_DATA_MAX ? len : IPC_DATA_MAX;
    if (data && len > 0) memcpy(msg.data, data, msg.len);

    if (ipc_send(ch, &msg) < 0) return -1;
    if (reply) return ipc_recv_timeout(ch, reply, 5000);
    return 0;
}
