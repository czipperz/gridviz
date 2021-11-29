#define NETGRIDVIZ_DEFINE
#include "netgridviz.h"

#include <stdio.h>
#include <random>

int main() {
    // Connect to server.
    int result = netgridviz_connect(NETGRIDVIZ_DEFAULT_PORT);
    if (result != 0) {
        fprintf(stderr, "Failed to connect!\n");
        return 1;
    }

    // Setup state.
    netgridviz_context normal = netgridviz_create_context();
    netgridviz_context destroyed = netgridviz_create_context();
    netgridviz_set_fg(&destroyed, 0xff, 0, 0);

    // Random number generators.
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> parse_dist(0, 1);
    std::uniform_int_distribution<int> destroy_dist(0, 3);

    // Make a demo grid.
    char grid[10][10] = {};

    // Visualize a grid as you parse it.
    netgridviz_start_stroke("Parse");
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            grid[y][x] = (parse_dist(gen) ? '#' : '.');
            netgridviz_draw_char(&normal, x, y, grid[y][x]);
        }
    }
    netgridviz_end_stroke();

    // Make some random changes.  Note: each draw command is sent as a separate stroke.
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            if (grid[y][x] == '#') {
                if (destroy_dist(gen) == 0) {
                    grid[y][x] = '.';
                    netgridviz_draw_char(&destroyed, x, y, grid[y][x]);
                }
            }
        }
    }

    netgridviz_disconnect();
}
