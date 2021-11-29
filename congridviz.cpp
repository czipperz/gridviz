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
    std::uniform_int_distribution<int> parse_dist(0, 1);
    std::uniform_int_distribution<int> destroy_dist(0, 3);

    netgridviz_context normal = netgridviz_create_context();
    netgridviz_context destroyed = netgridviz_create_context();

    netgridviz_set_fg(&destroyed, 0xff, 0, 0);

    char grid[10][10];

    netgridviz_start_stroke("Parse");
    // Visualize a grid as you parse it.
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid[y][x] = (parse_dist(rd) ? '#' : '.');
            netgridviz_draw_char(&normal, x, y, grid[y][x]);
        }
    }
    netgridviz_end_stroke();

    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            if (grid[y][x] == '#') {
                if (destroy_dist(rd) == 0) {
                    grid[y][x] = '.';
                    netgridviz_draw_char(&destroyed, x, y, grid[y][x]);
                }
            }
        }
    }

    netgridviz_disconnect();
}
