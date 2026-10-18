#pragma once

#include <cstdint>

namespace MyUnix::Memory::VMM {

constexpr uint64_t PAGE_SIZE = 4096;

enum PageFlags : uint64_t {
    Present  = 1ULL << 0,
    Writable = 1ULL << 1,
};

bool initialize(uint64_t hhdm_offset);

bool map_page(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t flags
);

bool unmap_page(uint64_t virtual_address);

bool translate(
    uint64_t virtual_address,
    uint64_t& physical_address
);

}