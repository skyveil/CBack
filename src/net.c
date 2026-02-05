#include "net.h"

#include "alloc.h"
#include <errno.h>
#include <stdlib.h>

#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/epoll.h>

// FIX: Nothing is error handled correctly, yet

void loop_add_net(cback_net_loop *loop, cback_net_conn *net);
void close_conn(cback_net_loop *loop, cback_net_conn *net);
int flush_out_buf(cback_net_loop *loop, cback_net_conn *net);

cback_net_conn *cback_net_connect(cback_net_loop *loop, const char *host, const char *port) {
    int sock_fd;

    struct addrinfo hints = { 0 }, *ai_list;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    cback_net_conn *net = malloc(sizeof(cback_net_conn));

    if (getaddrinfo(host, port, &hints, &ai_list) != 0) {
        net->state = NET_SOCK_UNINIT;
        return net;
    }

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

    if (conn_err != EINPROGRESS) {
        net->state = NET_SOCK_UNINIT;
        return net;
    }

    net->sock_fd = sock_fd;
    net->state = NET_CONNECTING;
    net->listen = 0;

    net->conn_arena = cback_arena_create(65536);

    net->rb_size = 8192;
    net->read_buf = cback_arena_alloc(&net->conn_arena, net->rb_size);

    net->ob_size = 8192;
    net->out_buf = cback_arena_alloc(&net->conn_arena, net->ob_size);

    net->read_len = 0;
    net->out_len = 0;

    loop_add_net(loop, net);

    return net;
}

int cback_net_send(cback_net_loop *loop, cback_net_conn *net, void *data, u32 size) {
    if (net->out_len > 0) {
        memcpy(net->out_buf, data, size);
        net->out_len += size;
        return size;
    }

    u32 nbytes = send(net->sock_fd, data, size, 0);
    if (nbytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            nbytes = 0;
        else
            return -1;
    }

    if (nbytes < size) {
        memcpy(net->out_buf, data + nbytes, size - nbytes);
        net->out_len += size - nbytes;

        struct epoll_event ev;
        ev.data.fd = net->sock_fd;
        ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
        if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, net->sock_fd, &ev) == -1) {
            loop->state = NET_LOOP_ADD_ERROR;
            return -1;
        }
    }

    return nbytes;
}

// Implement polling and manage sane reads and writes
cback_net_loop cback_net_loop_init(u8 max_conn) {
    int epoll_fd;

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        return (cback_net_loop){ .state = NET_LOOP_UNINIT };

    return (cback_net_loop){
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

    loop->state = NET_LOOP_OK;
    for (int i = 0; i < nfd; i++) {
        cback_net_conn *net = (cback_net_conn *)events[i].data.ptr;

        // When the library is in a decent state, handle errors earnestly
        if (events[i].events & (EPOLLHUP | EPOLLERR | EPOLLHUP)) {
            net->on_close(net);

            if (events[i].events & EPOLLERR) {
                int flag = 0;
                socklen_t len = sizeof(flag);
                getsockopt(net->sock_fd, SOL_SOCKET, SO_ERROR, &flag, &len);
            }

            if (events[i].events & EPOLLHUP) {
                // send remaining data
            }

            close_conn(loop, net);
        }

        if (events[i].events & EPOLLOUT) {
            if (net->state == NET_CONNECTING) {
                net->state = NET_CONNECTED;
                if (net->on_connect)
                    net->on_connect(loop, net);
            }

            if (net->out_len > 0)
                flush_out_buf(loop, net);
        }

        if (events[i].events & EPOLLIN) {
            // Handle recv repeatedly until errno = EAGAIN is set
            // Also handle connection failures/ending EPOLLHUP and writes EPOLLOUT
            u32 read_size = 0;
            while ((read_size = recv(net->sock_fd, net->read_buf + net->read_len, net->rb_size, 0)) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));

            net->read_len += read_size;
            if (net->on_data)
                net->on_data(net, net->read_buf, net->read_len);
        }
    }

    return 0;
}

int flush_out_buf(cback_net_loop *loop, cback_net_conn *net) {
    u32 nbytes = send(net->sock_fd, net->out_buf, net->out_len, 0);
    if (nbytes == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            nbytes = 0;
        else
            return -1;
    }

    memmove(net->out_buf, net->out_buf + nbytes, net->out_len - nbytes);
    net->out_len -= nbytes;

    if (net->out_len == 0) {
        struct epoll_event ev;
        ev.data.fd = net->sock_fd;
        ev.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, net->sock_fd, &ev) == -1) {
            loop->state = NET_LOOP_ADD_ERROR;
            return -1;
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

    loop->count++;

    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    ev.data.fd = net->sock_fd;
    ev.data.ptr = net;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, net->sock_fd, &ev) == -1) {
        loop->state = NET_LOOP_ADD_ERROR;
        return;
    }
}

void close_conn(cback_net_loop *loop, cback_net_conn *net) {
    if (close(net->sock_fd) == -1) {
        loop->state = NET_LOOP_DEL_ERROR;
        return;
    }

    cback_arena_destroy(&net->conn_arena);
    struct epoll_event temp;
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, net->sock_fd, &temp) == -1) {
        loop->state = NET_LOOP_DEL_ERROR;
        return;
    }

    free(net);
    net = NULL;
}
