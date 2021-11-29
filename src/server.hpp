#pragma once

namespace gridviz {

struct Network_State;
struct Game_State;

Network_State* start_networking(int port);
void poll_network(Network_State* net, Game_State* game);
void stop_networking(Network_State* net);

}
