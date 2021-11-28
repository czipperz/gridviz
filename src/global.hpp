#pragma once

#include <cz/string.hpp>

namespace gridviz {

///////////////////////////////////////////////////////////////////////////////
// Module Data
///////////////////////////////////////////////////////////////////////////////

extern cz::Allocator temp_allocator;
extern cz::Allocator permanent_allocator;

extern cz::Str program_name;
extern cz::Str program_directory;

///////////////////////////////////////////////////////////////////////////////
// Module Code
///////////////////////////////////////////////////////////////////////////////

void set_program_name(cz::Str fallback);
void set_program_directory();

}