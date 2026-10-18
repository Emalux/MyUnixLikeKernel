
#include "vmm.hpp"
#include "pmm.hpp"

#include <limine.h>
#include <cstdint>

namespace MyUnix::Memory::VMM {

static constexpr uint64_t ADDRESS_MASK =
    0x000FFFFFFFFFF000ULL;

static constexpr uint64_t ENTRY_PRESENT = 1ULL << 0;
static constexpr uint64_t ENTRY_WRITABLE = 1ULL << 1;
static constexpr uint64_t ENTRY_HUGE = 1ULL << 7;
static constexpr uint64_t ENTRY_USER = 1ULL << 2;

static constexpr uint64_t PHYSICAL_LIMIT =
    1ULL << 52;

static uint64_t hhdm_offset = 0;
static uint64_t root_physical = 0;
static bool ready = false;

static bool canonical(uint64_t address) {
    // Suporte inicial a endereços canônicos de 48 bits.
    uint64_t upper = address >> 48;
    bool sign = ((address >> 47) & 1) != 0;

    return sign ? upper == 0xFFFF : upper == 0;
}

static uint64_t* table_pointer(uint64_t physical) {
    if (physical >= PHYSICAL_LIMIT ||
        physical > UINT64_MAX - hhdm_offset) {
        return nullptr;
    }

    uint64_t virtual_address = hhdm_offset + physical;

    if (!canonical(virtual_address)) {
        return nullptr;
    }

    return reinterpret_cast<uint64_t*>(virtual_address);
}

static uint64_t read_cr3() {
    uint64_t value;
    asm volatile("mov %%cr3, %0" : "=r"(value));
    return value;
}

static uint64_t read_cr4() {
    uint64_t value;
    asm volatile("mov %%cr4, %0" : "=r"(value));
    return value;
}

static void invalidate(uint64_t address) {
    asm volatile("invlpg (%0)" :: "r"(address) : "memory");
}

static uint64_t* allocate_table() {
    uint64_t physical = MyUnix::Memory::allocate_page();

    if (physical == 0) {
        return nullptr;
    }

    uint64_t* table = table_pointer(physical);

    if (table == nullptr) {
        // Não liberamos a página aqui: evitar reutilizar uma
        // página cuja acessibilidade não pudemos confirmar.
        return nullptr;
    }

    for (size_t i = 0; i < 512; ++i) {
        table[i] = 0;
    }

    return table;
}

static uint64_t* next_table(uint64_t* parent,
                            size_t index,
                            bool create) {
    uint64_t entry = parent[index];

    if ((entry & ENTRY_PRESENT) == 0) {
        if (!create) {
            return nullptr;
        }

        uint64_t* child = allocate_table();

        if (child == nullptr) {
            return nullptr;
        }

        uint64_t physical =
            reinterpret_cast<uint64_t>(child) - hhdm_offset;

        parent[index] =
            (physical & ADDRESS_MASK) |
            ENTRY_PRESENT | ENTRY_WRITABLE;

        return child;
    }

    // Um mapeamento de página enorme não é uma tabela filha.
    if (entry & ENTRY_HUGE) {
        return nullptr;
    }

    return table_pointer(entry & ADDRESS_MASK);
}

bool initialize(uint64_t offset) {
    if (ready || (read_cr4() & (1ULL << 12))) {
        // O bit LA57 indica paginação de cinco níveis.
        return false;
    }

    hhdm_offset = offset;
    root_physical = read_cr3() & ADDRESS_MASK;

    if (table_pointer(root_physical) == nullptr) {
        return false;
    }

    ready = true;
    return true;
}

bool map_page(uint64_t virtual_address,
              uint64_t physical_address,
              uint64_t flags) {
    if (!ready ||
        !canonical(virtual_address) ||
        (virtual_address & (PAGE_SIZE - 1)) != 0 ||
        (physical_address & (PAGE_SIZE - 1)) != 0 ||
        physical_address >= PHYSICAL_LIMIT ||
        (flags & ~Writable) != 0) {
        return false;
    }

    uint64_t* pml4 = table_pointer(root_physical);

    if (pml4 == nullptr) {
        return false;
    }

    size_t i4 = (virtual_address >> 39) & 0x1FF;
    size_t i3 = (virtual_address >> 30) & 0x1FF;
    size_t i2 = (virtual_address >> 21) & 0x1FF;
    size_t i1 = (virtual_address >> 12) & 0x1FF;

    uint64_t* pdpt = next_table(pml4, i4, true);
    if (!pdpt) return false;

    uint64_t* pd = next_table(pdpt, i3, true);
    if (!pd) return false;

    uint64_t* pt = next_table(pd, i2, true);
    if (!pt) return false;

    // Não substitui silenciosamente um mapeamento existente.
    if (pt[i1] & ENTRY_PRESENT) {
        return false;
    }

    pt[i1] = (physical_address & ADDRESS_MASK) |
             ENTRY_PRESENT |
             (flags & Writable);

    invalidate(virtual_address);
    return true;
}

bool unmap_page(uint64_t virtual_address) {
    if (!ready ||
        !canonical(virtual_address) ||
        (virtual_address & (PAGE_SIZE - 1)) != 0) {
        return false;
    }

    uint64_t* pml4 = table_pointer(root_physical);
    if (!pml4) return false;

    size_t i4 = (virtual_address >> 39) & 0x1FF;
    size_t i3 = (virtual_address >> 30) & 0x1FF;
    size_t i2 = (virtual_address >> 21) & 0x1FF;
    size_t i1 = (virtual_address >> 12) & 0x1FF;

    uint64_t* pdpt = next_table(pml4, i4, false);
    if (!pdpt) return false;

    uint64_t* pd = next_table(pdpt, i3, false);
    if (!pd) return false;

    uint64_t* pt = next_table(pd, i2, false);
    if (!pt || !(pt[i1] & ENTRY_PRESENT)) {
        return false;
    }

    pt[i1] = 0;
    invalidate(virtual_address);
    return true;
}

bool translate(uint64_t virtual_address,
               uint64_t& physical_address) {
    if (!ready || !canonical(virtual_address)) {
        return false;
    }

    uint64_t* pml4 = table_pointer(root_physical);
    if (!pml4) return false;

    size_t i4 = (virtual_address >> 39) & 0x1FF;
    size_t i3 = (virtual_address >> 30) & 0x1FF;
    size_t i2 = (virtual_address >> 21) & 0x1FF;
    size_t i1 = (virtual_address >> 12) & 0x1FF;

    uint64_t e4 = pml4[i4];
    if (!(e4 & ENTRY_PRESENT)) return false;

    uint64_t* pdpt = table_pointer(e4 & ADDRESS_MASK);
    if (!pdpt) return false;

    uint64_t e3 = pdpt[i3];
    if (!(e3 & ENTRY_PRESENT) || (e3 & ENTRY_HUGE)) {
        return false;
    }

    uint64_t* pd = table_pointer(e3 & ADDRESS_MASK);
    if (!pd) return false;

    uint64_t e2 = pd[i2];
    if (!(e2 & ENTRY_PRESENT) || (e2 & ENTRY_HUGE)) {
        return false;
    }

    uint64_t* pt = table_pointer(e2 & ADDRESS_MASK);
    if (!pt) return false;

    uint64_t e1 = pt[i1];
    if (!(e1 & ENTRY_PRESENT)) return false;

    physical_address = (e1 & ADDRESS_MASK) |
                       (virtual_address & (PAGE_SIZE - 1));
    return true;
}

}
