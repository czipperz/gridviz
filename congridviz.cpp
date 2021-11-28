#define NETGRIDVIZ_DEFINE
#include "netgridviz.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "src/event.hpp"

int main() {
    int result = netgridviz_connect(NETGRIDVIZ_DEFAULT_PORT);
    if (result != 0) {
        fprintf(stderr, "Failed to connect!\n");
        return 1;
    }

    srand(time(0));

    gridviz::Event event = {};
    event.cp.type = gridviz::EVENT_CHAR_POINT;
    event.cp.fg[0] = 0xff;
    event.cp.fg[1] = 0xff;
    event.cp.fg[2] = 0xff;
    event.cp.bg[0] = 0x00;
    event.cp.bg[1] = 0x00;
    event.cp.bg[2] = 0x00;
    event.cp.ch = '#';
    event.cp.x = (rand() % 10);
    event.cp.y = (rand() % 10);

    netgridviz_send(&event, sizeof(event));

    netgridviz_disconnect();
}
