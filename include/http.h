#ifndef HTTP_H
#define HTTP_H

#include "net.h"

typedef enum {
    HTTP_GET = 1,
    HTTP_POST,
    HTTP_PUT,
} cback_http_method;

typedef struct {
    char *data;
    u32 len;
} _cback_string_view;

typedef struct cback_http_header {
    _cback_string_view key;
    _cback_string_view value;
} cback_http_header;

typedef struct cback_http_req {
    cback_http_method method;
    cback_http_header *headers;

    _cback_string_view body;
} cback_http_req;

typedef struct cback_http_res {
    u16 status;
    _cback_string_view status_string;
    cback_http_method method;

    u32 num_headers;
    cback_http_header *headers;
    _cback_string_view body;
} cback_http_res;

typedef void (*cback_http_cb)(cback_http_req *req, cback_http_res *res);
void cback_http_get(cback_net_loop *loop, char *url, cback_http_cb cb, cback_http_req *req);

#endif
