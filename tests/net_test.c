#include "net.h"

#include <stdio.h>
#include <stdlib.h>

void on_data(cback_net_loop *loop, cback_net_conn *net) {
    printf("Got %d bytes!!!\n", net->read_len);
    char *data = (char *)net->read_buf;

    for (int i = 0; i < net->read_len; i++) {
        putc(data[i], stdout);
    }
}

void on_connect(cback_net_loop *loop, cback_net_conn *net) {
    char message[] = "GET / HTTP/1.1\r\nConnection: close\r\nHost: monkeytype.com\r\n\r\n";
    printf("Sending request\n");
    cback_net_send(loop, net, message, sizeof(message));
    printf("Request sent!!!\n");
}

int main() {
    cback_net_loop loop = cback_net_loop_init(12);
    cback_net_conn *net = cback_net_connect(&loop, "monkeytype.com", "443", NET_PROTO_SSL);
    if (net->state != NET_SSL_HANDSHAKE) {
        perror("");
        exit(EXIT_FAILURE);
    }

    net->on_data = &on_data;
    net->on_connect = &on_connect;
    printf("Successfully waiting for connection!!!\n");
    char prit = 1;
    while (1) {
        if (net->state == NET_CONNECTED && prit)
            printf("Connected!!!"), prit = 0;
        cback_net_loop_poll(&loop, 100);
    }
}
