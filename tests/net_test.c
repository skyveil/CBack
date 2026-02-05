#include "net.h"

#include <stdio.h>
#include <stdlib.h>

void on_data(cback_net_conn *net, void *recv_data, u32 len) {
    printf("Got %d bytes!!!\n", len);
    char *data = recv_data;

    for (int i = 0; i < len; i++) {
        printf("%c", data[i]);
    }
}

void on_connect(cback_net_loop *loop, cback_net_conn *net) {
    char message[17] = "GET / HTTP/1.1\r\n";
    cback_net_send(loop, net, message, 17);
}

int main() {
    cback_net_loop loop = cback_net_loop_init(12);
    cback_net_conn *net = cback_net_connect(&loop, "httpforever.com", "80");
    if (net->state != NET_CONNECTING) {
        perror("");
        exit(EXIT_FAILURE);
    }

    net->on_data = &on_data;
    net->on_connect = &on_connect;
    printf("Successfully waiting for connection!!!\n");
    while (1) {
        if (net->state == NET_DISCONNECTED) {
            printf("Connected successfully!!!\n");
            exit(EXIT_SUCCESS);
        }

        cback_net_loop_poll(&loop, 100);
    }
}
