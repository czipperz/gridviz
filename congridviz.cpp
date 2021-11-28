#define NETGRIDVIZ_DEFINE
#include "netgridviz.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "src/event.hpp"

int main() {
    int result = netgridviz_connect(NETGRIDVIZ_DEFAULT_PORT);
    if (result != 0) {
        fprintf(stderr, "Failed to connect!\n");
        return 1;
    }

    srand(time(0));

    netgridviz_context c1 = netgridviz_create_context();
    netgridviz_context c2 = netgridviz_create_context();

    netgridviz_set_fg(&c1, 0xff, 0, 0);

    netgridviz_send_char(&c1, /*x=*/(rand() % 10), /*y=*/(rand() % 10), '#');
    netgridviz_send_char(&c2, /*x=*/(rand() % 10), /*y=*/(rand() % 10), 'X');

    netgridviz_disconnect();
}
