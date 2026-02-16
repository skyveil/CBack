#ifndef HTTP_H
#define HTTP_H

#include "net.h"

typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
} cback_http_method;

typedef struct {
    char *ptr;
    u32 len;
} _string_view;

typedef struct _http_url {
    char *host;
    char *port;
    char *path;
    cback_net_proto proto;
} _http_url;

typedef struct cback_http_header {
    char *key;
    char *value;
} cback_http_header;

// user data
typedef struct cback_http_req {
    cback_http_method method;
    cback_http_header *headers;
    char *url;

    u32 body_len;
    char *body;
} cback_http_req;

// internal data
typedef struct cback_http {
    cback_http_req *req;
    _http_url url;

    cback_net_loop *loop;
    cback_arena arena;
} cback_http;

cback_http cback_http_send(cback_net_loop *loop, cback_http_req *req);

#endif
