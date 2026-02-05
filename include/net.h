#ifndef NET_H
#define NET_H

#include "alloc.h"
#include "utils.h"

#include <sys/socket.h>

typedef enum {
    NET_OK = 0,
    NET_CONNECTING,
    NET_CONNECTED,
    NET_DISCONNECTED,

    NET_SOCK_UNINIT = -1,
    NET_NO_LOOP = -2,
} cback_net_state;

struct cback_net_conn;
struct cback_net_loop;

typedef void (*cback_net_data_cb)(struct cback_net_conn *net, void *data, u32 len);
typedef void (*cback_net_close_cb)(struct cback_net_conn *net);
typedef void (*cback_net_connect_cb)(struct cback_net_loop *loop, struct cback_net_conn *net);

typedef struct cback_net_conn {
    int sock_fd;
    int listen;
    cback_net_state state;

    cback_arena conn_arena;

    u32 rb_size;
    u8 *read_buf;
    u32 read_len;

    u32 ob_size;
    u8 *out_buf;
    u32 out_len;

    cback_net_data_cb on_data;
    cback_net_close_cb on_close;
    cback_net_connect_cb on_connect;
} cback_net_conn;

typedef enum {
    NET_LOOP_OK = 0,

    NET_LOOP_UNINIT = -1,
    NET_LOOP_MAX_CONN = -2,
    NET_LOOP_ADD_ERROR = -3,
    NET_LOOP_POLL_ERROR = -4,
    NET_LOOP_DEL_ERROR = -5,
} cback_net_loop_state;

// NOTE: Current implementation uses an array
//       A HashMap is preffered ("collections.h")
typedef struct cback_net_loop {
    int epoll_fd;

    u32 max_conn;
    u32 count;
    cback_net_loop_state state;
} cback_net_loop;

cback_net_conn *cback_net_connect(cback_net_loop *loop, const char *host, const char *port);
int cback_net_send(cback_net_loop *loop, cback_net_conn *net, void *data, u32 size);

cback_net_loop cback_net_loop_init(u8 max_conn);
int cback_net_loop_poll(cback_net_loop *loop, u16 timeout);

#endif
