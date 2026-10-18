
#include "heap.hpp"
#include "pmm.hpp"
#include "vmm.hpp"

#include <cstddef>
#include <cstdint>

namespace MyUnix::Memory::Heap {

static constexpr uint64_t PAGE_SIZE = 4096;

// Região virtual reservada para o heap do kernel.
// Confirme que não colide com os mapeamentos existentes.
static constexpr uint64_t HEAP_BASE =
    0xFFFFC00000000000ULL;

static constexpr uint64_t MAX_HEAP_PAGES = 4096; // 16 MiB
static constexpr std::size_t ALIGNMENT = 16;

struct Block {
    std::size_t size;
    bool free;
    Block* next;
    Block* prev;
};

static Block* first = nullptr;
static Block* last = nullptr;
static uint64_t page_count = 0;
static bool ready = false;

static std::size_t align_up(std::size_t value) {
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

static bool adjacent(Block* left, Block* right) {
    auto end = reinterpret_cast<uintptr_t>(left)
             + sizeof(Block) + left->size;

    return end == reinterpret_cast<uintptr_t>(right);
}

static void merge_next(Block* block) {
    Block* next = block->next;

    if (!next || !next->free || !adjacent(block, next)) {
        return;
    }

    block->size += sizeof(Block) + next->size;
    block->next = next->next;

    if (next->next) {
        next->next->prev = block;
    } else {
        last = block;
    }
}

static bool expand() {
    if (page_count >= MAX_HEAP_PAGES) {
        return false;
    }

    const uint64_t virtual_address =
        HEAP_BASE + page_count * PAGE_SIZE;

    // Não sobrescreva um mapeamento já existente.
    uint64_t existing_physical = 0;

    if (MyUnix::Memory::VMM::translate(
            virtual_address, existing_physical)) {
        return false;
    }

    const uint64_t physical =
        MyUnix::Memory::allocate_page();

    if (physical == 0) {
        return false;
    }

    if (!MyUnix::Memory::VMM::map_page(
            virtual_address,
            physical,
            MyUnix::Memory::VMM::Writable)) {
        MyUnix::Memory::free_page(physical);
        return false;
    }

    auto* block = reinterpret_cast<Block*>(virtual_address);

    block->size = PAGE_SIZE - sizeof(Block);
    block->free = true;
    block->next = nullptr;
    block->prev = last;

    if (last) {
        last->next = block;
    } else {
        first = block;
    }

    last = block;
    ++page_count;

    // Pode unir a nova página à região livre anterior.
    if (block->prev && block->prev->free) {
        merge_next(block->prev);
    }

    return true;
}

bool initialize() {
    if (ready) {
        return false;
    }

    first = nullptr;
    last = nullptr;
    page_count = 0;
    ready = true;

    // A primeira página será mapeada sob demanda.
    return true;
}

void* allocate(std::size_t size) {
    if (!ready) {
        return nullptr;
    }

    if (size == 0) {
        size = 1;
    }

    if (size > SIZE_MAX - (ALIGNMENT - 1)) {
        return nullptr;
    }

    size = align_up(size);

    if (size > SIZE_MAX - sizeof(Block)) {
        return nullptr;
    }

    while (true) {
        for (Block* block = first; block; block = block->next) {
            if (!block->free || block->size < size) {
                continue;
            }

            const std::size_t remaining =
                block->size - size;

            // Só dividir se restar espaço para outro bloco útil.
            if (remaining >= sizeof(Block) + ALIGNMENT) {
                auto* split = reinterpret_cast<Block*>(
                    reinterpret_cast<uintptr_t>(block)
                    + sizeof(Block) + size
                );

                split->size = remaining - sizeof(Block);
                split->free = true;
                split->prev = block;
                split->next = block->next;

                if (split->next) {
                    split->next->prev = split;
                } else {
                    last = split;
                }

                block->next = split;
                block->size = size;
            }

            block->free = false;

            return reinterpret_cast<void*>(
                reinterpret_cast<uintptr_t>(block)
                + sizeof(Block)
            );
        }

        if (!expand()) {
            return nullptr;
        }
    }
}

bool deallocate(void* pointer) {
    if (!ready || pointer == nullptr) {
        return false;
    }

    const uintptr_t address =
        reinterpret_cast<uintptr_t>(pointer);

    const uintptr_t begin = HEAP_BASE;
    const uintptr_t end = HEAP_BASE + page_count * PAGE_SIZE;

    if (address < begin + sizeof(Block) || address >= end) {
        return false;
    }

    auto* block = reinterpret_cast<Block*>(
        address - sizeof(Block)
    );

    // Confirme que o ponteiro veio deste alocador.
    bool found = false;

    for (Block* current = first; current; current = current->next) {
        if (current == block) {
            found = true;
            break;
        }
    }

    if (!found || block->free) {
        return false;
    }

    block->free = true;

    if (block->prev && block->prev->free) {
        block = block->prev;
        merge_next(block);
    }

    merge_next(block);

    return true;
}

}
