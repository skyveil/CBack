#include "net.h"

#include <errno.h>
#include <stdlib.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>

void loop_add_net(cback_net_loop *loop, cback_net_conn *net);

cback_net_conn *_net_state(cback_net_conn *net, cback_net_state state);
cback_net_loop _net_loop_state(cback_net_loop_state state);

cback_net_conn *cback_net_connect(cback_net_loop *loop, const char *host, const char *port) {
    int sock_fd;

    struct addrinfo hints = {0}, *ai_list;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    cback_net_conn *net = malloc(sizeof(cback_net_conn));

    if (getaddrinfo(host, port, &hints, &ai_list) != 0)
        return _net_state(net, NET_SOCK_UNINIT);

    int conn_err = 0;
    for (struct addrinfo *ai = ai_list; ai->ai_next != NULL; ai = ai->ai_next) {
        sock_fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);
        if (sock_fd == -1)
            continue;

        int flag = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &flag, sizeof(flag)) == -1)
            continue;

        if (connect(sock_fd, ai->ai_addr, ai->ai_addrlen) == -1 && (conn_err = errno) != EINPROGRESS)
            continue;

        break;
    }

    if (conn_err != EINPROGRESS)
        return _net_state(net, NET_SOCK_UNINIT);

    net->sock_fd = sock_fd;
    net->state = NET_CONNECTING;
    net->listen = 0;

    loop_add_net(loop, net);

    return net;
}

// Implement polling and manage sane reads and writes
cback_net_loop cback_net_loop_init(u8 max_conn) {
    int epoll_fd;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        return _net_loop_state(NET_LOOP_UNINIT);

    return (cback_net_loop){
        .connections = NULL,
        .state = NET_LOOP_OK,
        .epoll_fd = epoll_fd,
        .max_conn = max_conn,
        .count = 0,
    };
}

int cback_net_loop_poll(cback_net_loop *loop, u16 timeout) {
    struct epoll_event events[loop->max_conn];
    int nfd = epoll_wait(loop->epoll_fd, events, loop->max_conn, timeout);
    if (nfd == -1) {
        loop->state = NET_LOOP_POLL_ERROR;
        return -1;
    }

    for (int i = 0; i < nfd; i++) {
        for (int j = 0; j < loop->count; j++) {
            if (events[i].data.fd == loop->connections[j].sock_fd) {
                int error = 0;
                socklen_t len = sizeof(error);

                if (getsockopt(loop->connections[j].sock_fd, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
                    loop->state = NET_LOOP_POLL_ERROR;
                    continue;
                }

                if (error != 0) continue;

                loop->connections[i].state = NET_CONNECTED;
            }
        }
    }

    return 0;
}

void loop_add_net(cback_net_loop *loop, cback_net_conn *net) {
    if (loop == NULL) {
        net->state = NET_NO_LOOP;
        return;
    }

    if (loop->count >= loop->max_conn) {
        loop->state = NET_LOOP_MAX_CONN;
        return;
    }

    if (loop->connections == NULL)
        loop->connections = net;
    else
        loop->connections->next = net;
    loop->count++;

    struct epoll_event ev;
    ev.events = EPOLLOUT;
    ev.data.fd = net->sock_fd;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, net->sock_fd, &ev) == -1) {
        loop->state = NET_LOOP_ADD_ERROR;
        return;
    }
}

cback_net_conn *_net_state(cback_net_conn *net, cback_net_state state) {
    net->state = state;
    return net;
}

cback_net_loop _net_loop_state(cback_net_loop_state state) {
    return (cback_net_loop){.state = state};
}
