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

    netgridviz_context context = netgridviz_create_context();
    netgridviz_send_char(&context, /*x=*/(rand() % 10), /*y=*/(rand() % 10), '#');

    netgridviz_disconnect();
}
