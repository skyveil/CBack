#include "http.h"
#include "net.h"

_http_url parse_url(char *url);
char *init_conn(_http_url *url, cback_arena *arena);

void send_request(cback_net_loop *loop, cback_net_conn *net);
void on_data(cback_net_loop *loop, cback_net_conn *net);

cback_http cback_http_send(cback_net_loop *loop, cback_http_req *req) {
    _http_url url = parse_url(req->url);
    cback_net_conn *net = cback_net_connect(loop, url.host, url.port, url.proto);
    // implement on_data and on_connect to get the response and send the initial request

    cback_arena arena = cback_arena_create(65536);

    net->user_data = init_conn(&url, &arena);
    net->on_connect = &send_request;
    net->on_data = &on_data;

    return (cback_http){};
}

void send_request(cback_net_loop *loop, cback_net_conn *net) {
    char *req = net->user_data;

    cback_net_send(loop, net, req, strlen(req));
}

void on_data(cback_net_loop *loop, cback_net_conn *net) {
    char *data = (char *)net->read_buf;

    for (int i = 0; i < net->read_len; i++) {
        printf("%c", data[i]);
    }
}

char *init_conn(_http_url *url, cback_arena *arena) {
    u32 size =  128 + strlen(url->path) + strlen(url->host);
    char *req = cback_arena_alloc(arena, size);
    snprintf(req, size,
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n\r\n",
        (url->path[0] == '\0' ? "/" : url->path),
        url->host
    );

    return req;
}

// Incomplete, but works for simple (and correct) uris
_http_url parse_url(char *url) {
    int idx = 0;
    _http_url http_url;

    if (!strncmp(url, "http", 4)) {
        idx += 4;

        if (url[4] == 's') {
            http_url.port = "443";
            http_url.proto = NET_PROTO_SSL;
            idx++;
        }
        else {
            http_url.port = "80";
            http_url.proto = NET_PROTO_RAW;
        }

        if (strncmp(url + idx, "://", 3)) {
            // printf("Incorrect URL :(");
            return http_url;
        }

        idx += 3;
    }
    else {
        http_url.port = "80";
        http_url.proto = NET_PROTO_RAW;
    }

    int host_idx = idx;
    while (url[idx] != '/' && url[idx] != '\0')
        idx++;

    http_url.host = malloc(idx - host_idx);
    http_url.path = malloc(strlen(url) - idx);

    memcpy(http_url.host, url + host_idx, idx - host_idx);
    memcpy(http_url.path, url + idx, strlen(url) - idx);

    return http_url;
}
