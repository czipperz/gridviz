#pragma once

#include <cz/string.hpp>
#include <cz/vector.hpp>
#include "event.hpp"

namespace gridviz {

struct Network_State;

Network_State* start_networking(int port);
void poll_network(Network_State* net, cz::Vector<Stroke>* strokes);
void stop_networking(Network_State* net);

}
