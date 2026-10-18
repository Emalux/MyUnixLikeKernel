#pragma once

#include <cstddef>

namespace MyUnix::Memory::Heap {

bool initialize();

void* allocate(std::size_t size);
bool deallocate(void* pointer);

}
