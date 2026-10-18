#pragma once

#include <cstddef>
#include <cstdint>

struct limine_memmap_response;

namespace MyUnix::Memory {

constexpr uint64_t PAGE_SIZE = 4096;
constexpr uint64_t PMM_LIMIT = 1024ULL * 1024 * 1024;
constexpr size_t PAGE_COUNT = PMM_LIMIT / PAGE_SIZE;
constexpr size_t BITMAP_SIZE = PAGE_COUNT / 8;

bool initialize(limine_memmap_response* map);
uint64_t allocate_page();
bool free_page(uint64_t physical_address);

uint64_t total_pages();
uint64_t free_pages();

}