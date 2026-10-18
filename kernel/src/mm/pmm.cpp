#include "pmm.hpp"
#include <limine.h>



namespace MyUnix::Memory {

static uint8_t bitmap[BITMAP_SIZE];
static uint64_t free_count = 0;
static bool initialized = false;

static bool is_used(size_t index) {
    return (bitmap[index / 8] & (1u << (index % 8))) != 0;
}

static void set_used(size_t index) {
    bitmap[index / 8] |= static_cast<uint8_t>(
        1u << (index % 8)
    );
}

static void set_free(size_t index) {
    bitmap[index / 8] &= static_cast<uint8_t>(
        ~(1u << (index % 8))
    );
}

bool initialize(limine_memmap_response* map) {
    if (initialized || map == nullptr) {
        return false;
    }

    // Por segurança, tudo começa como indisponível.
    for (size_t i = 0; i < BITMAP_SIZE; ++i) {
        bitmap[i] = 0xFF;
    }

    for (uint64_t i = 0; i < map->entry_count; ++i) {
        auto entry = map->entries[i];

        if (entry == nullptr ||
            entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        // Evita overflow em base + length.
        if (entry->base >= PMM_LIMIT ||
            entry->length == 0) {
            continue;
        }

        uint64_t end;
        if (entry->length > PMM_LIMIT - entry->base) {
            end = PMM_LIMIT;
        } else {
            end = entry->base + entry->length;
        }

        // Só liberamos páginas totalmente contidas
        // na região marcada como utilizável.
        uint64_t first =
            (entry->base + PAGE_SIZE - 1) / PAGE_SIZE;

        uint64_t last = end / PAGE_SIZE;

        if (first >= PAGE_COUNT) {
            continue;
        }

        if (last > PAGE_COUNT) {
            last = PAGE_COUNT;
        }

        for (uint64_t page = first; page < last; ++page) {
            if (page == 0 || !is_used(page)) {
                continue;
            }

            set_free(page);
            ++free_count;
        }
    }

    // A página física zero nunca será alocada.
    set_used(0);

    initialized = true;
    return true;
}

uint64_t allocate_page() {
    if (!initialized) {
        return 0;
    }

    for (size_t page = 1; page < PAGE_COUNT; ++page) {
        if (!is_used(page)) {
            set_used(page);
            --free_count;

            return static_cast<uint64_t>(page) * PAGE_SIZE;
        }
    }

    // Zero significa que não há página disponível.
    return 0;
}

bool free_page(uint64_t physical_address) {
    if (!initialized ||
        physical_address == 0 ||
        physical_address >= PMM_LIMIT ||
        physical_address % PAGE_SIZE != 0) {
        return false;
    }

    size_t page = physical_address / PAGE_SIZE;

    if (!is_used(page)) {
        return false;
    }

    set_free(page);
    ++free_count;

    return true;
}

uint64_t total_pages() {
    return PAGE_COUNT;
}

uint64_t free_pages() {
    return free_count;
}

}