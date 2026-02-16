#include "http.h"
#include "net.h"

int main() {
    cback_net_loop loop = cback_net_loop_init(12);
    cback_http_req req = {
        .url = "https://monkeytype.com/leaderboards"
    };

    cback_http http = cback_http_send(&loop, &req);

    while (1) {
        cback_net_loop_poll(&loop, 1000);
    }
}
