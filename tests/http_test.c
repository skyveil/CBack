#include "http.h"
#include "net.h"

void mkt_callback(cback_http_req *req, cback_http_res *res) {
    printf("Status: %d %.*s\n", res->status, res->status_string.len, res->status_string.data);

    printf("Got following response:\n{\n");
    for (int i = 0; i < res->num_headers; i++) {
        printf("\t\"%.*s\": \"%.*s\"\n", res->headers[i].key.len, res->headers[i].key.data, res->headers[i].value.len, res->headers[i].value.data);
    }
    printf("}\n");

    printf("\nBody:\n");
    printf("%.*s\n", res->body.len, res->body.data);

    exit(EXIT_SUCCESS);
}

int main() {
    cback_net_loop loop = cback_net_loop_init(12);
    cback_http_get(&loop, "https://monkeytype.com/", mkt_callback, NULL);

    while (1) {
        cback_net_loop_poll(&loop, 1000);
    }
}
