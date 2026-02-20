#include "http.h"
#include "alloc.h"
#include "net.h"
#include <string.h>

typedef struct _http_url {
    char *host;
    char *port;
    char *path;
    cback_net_proto proto;
} _http_url;

_http_url parse_url(char *url);
cback_http_res *parse_response(char *data, cback_arena *arena);
char *form_header_string(cback_http_req *req, _http_url *url, cback_arena *arena);

void get_send(cback_net_loop *loop, cback_net_conn *net);
void get_recv(cback_net_loop *loop, cback_net_conn *net);

void cback_http_get(cback_net_loop *loop, char *url, cback_http_cb cb, cback_http_req *req) {
    _http_url http_url = parse_url(url);
    cback_net_conn *net = cback_net_connect(loop, http_url.host, http_url.port, http_url.proto);

    cback_http_req request;
    if (req != NULL) request = *req;

    request.method = HTTP_GET;

    void **user_data = cback_arena_alloc(&net->conn_arena, sizeof(void *) * 3);
    user_data[0] = form_header_string(&request, &http_url, &net->conn_arena);
    user_data[1] = &request;
    user_data[2] = cb;

    net->user_data = user_data;
    net->on_connect = &get_send;
    net->on_data = &get_recv;
}

void get_send(cback_net_loop *loop, cback_net_conn *net) {
    void **user_data = (void **)net->user_data;
    char *req = user_data[0];

    cback_net_send(loop, net, req, strlen(req));
}

void get_recv(cback_net_loop *loop, cback_net_conn *net) {
    char *data = (char *)net->read_buf;
    void **user_data = (void **)net->user_data;
    cback_http_cb cb = user_data[2];
    cback_http_req *req = user_data[1];

    cback_http_res *response = parse_response(data, &net->conn_arena);
    if (cb != NULL)
        cb(req, response);
}

cback_http_res *parse_response(char *data, cback_arena *arena) {
    printf("Successfully engaged!!!\n");
    cback_http_res *response = cback_arena_alloc(arena, sizeof(cback_http_res));
    cback_http_header *headers = cback_arena_alloc(arena, sizeof(cback_http_header)), *header;
    header = headers;

    char *res = data;
    char *status_end = strstr(res, "\r\n");
    char *status_code = memchr(res, ' ', status_end - res);
    char *status_string = memchr(status_code + 1, ' ', status_code - res - 1);

    int status = 0;
    if (status_code) {
        char status_code_str[8];
        strncpy(status_code_str, status_code, status_string - status_code);
        status = atoi(status_code_str);
    }

    while (status_string[0] == ' ' || status_string[0] == '\t') {
        status_string++;
    }

    res = status_end;
    res += 2;

    u32 body_len;
    response->num_headers = 0;
    char *header_end = strstr(res, "\r\n\r\n");
    while (res < header_end) {
        char *lend = strstr(res, "\r\n");
        if (!lend || lend == res) break;

        response->num_headers++;

        char *colon = memchr(res, ':', lend - res);
        if (colon) {
            header->key = (_cback_string_view){res, colon - res};
            header->value = (_cback_string_view){colon + 1, lend - colon - 1};

            while (header->value.data[0] == ' ' || header->value.data[0] == '\t') {
                header->value.data++;
                header->value.len--;
            }

            if (body_len != 0 && strncmp(header->key.data, "Content-Length", 14) == 0) {
                char len[16];
                strncpy(len, header->value.data, header->value.len);

                body_len = atoi(len);
            }
        }

        ++header;
        header = cback_arena_alloc(arena, sizeof(cback_http_header));
        res = lend + 2;
    }

    res += 2;
    response->body = (_cback_string_view){res, body_len};
    response->status = status;

    response->status_string.data = cback_arena_alloc(arena, status_end - status_string);
    response->status_string.data = status_string;
    response->status_string.len = status_end - status_string;

    response->headers = headers;
    return response;
}

char *form_header_string(cback_http_req *request, _http_url *url, cback_arena *arena) {
    u32 size =  128 + strlen(url->path) + strlen(url->host);
    char *req = cback_arena_alloc(arena, size);

    char method[8] = {0};
    switch (request->method) {
        default:
        case HTTP_GET:
            strncpy(method, "GET", 3);
            break;

        case HTTP_PUT:
            strncpy(method, "PUT", 3);
            break;

        case HTTP_POST:
            strncpy(method, "POST", 4);
            break;
    }

    snprintf(req, size,
        "%s %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n\r\n",
        method,
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
