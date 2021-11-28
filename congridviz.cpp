#define NETGRIDVIZ_DEFINE
#include "netgridviz.h"

#include <stdio.h>
#include <random>

int main() {
    int result = netgridviz_connect(NETGRIDVIZ_DEFAULT_PORT);
    if (result != 0) {
        fprintf(stderr, "Failed to connect!\n");
        return 1;
    }

    std::random_device rd;
    std::uniform_int_distribution<int> dist(0, 9);

    netgridviz_context c1 = netgridviz_create_context();
    netgridviz_context c2 = netgridviz_create_context();

    netgridviz_set_fg(&c1, 0xff, 0, 0);

    netgridviz_send_char(&c1, /*x=*/dist(rd), /*y=*/dist(rd), '#');
    netgridviz_send_char(&c2, /*x=*/dist(rd), /*y=*/dist(rd), 'X');

    netgridviz_disconnect();
}
