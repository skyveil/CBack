#include "collections.h"
#include "net.h"

#include <stdio.h>

int main() {
    cback_hash_map *hm = cback_hashmap_create(1000, sizeof(int), sizeof(cback_net_conn *));

    printf("asdfasdfasdf\n");
    cback_net_conn *net = cback_net_connect(NULL, "youtube.com", "80", NET_PROTO_SSL);
    cback_hash_map_put(hm, &(net->sock_fd), net);

    cback_net_conn *net2 = cback_net_connect(NULL, "youtube.com", "80", NET_PROTO_SSL);
    cback_hash_map_put(hm, &(net2->sock_fd), net2);

    cback_net_conn *net3 = cback_net_connect(NULL, "youtube.com", "80", NET_PROTO_SSL);
    cback_hash_map_put(hm, &(net3->sock_fd), net3);

    printf("Looked for %d\n", net3->sock_fd);

    void *g_net;
    if (cback_hash_map_get(hm, &(net3->sock_fd), &g_net)) {
        printf("Got key: %d, value.fd: %d\n", net3->sock_fd, ((cback_net_conn *)g_net)->sock_fd);
    } else {
        fprintf(stderr, "Key not found!!!\n");
    }

    cback_hash_map_destroy(hm);
}
