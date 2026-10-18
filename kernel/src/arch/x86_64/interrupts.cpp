
#include "interrupts.hpp"

namespace MyUnix::Arch::X86_64::Interrupts {

    uint64_t read_rflags() {
        uint64_t value;

        asm volatile(
            "pushfq\n\t"
            "popq %0"
            : "=r"(value)
            :
            : "memory"
        );

        return value;
    }

    uint64_t read_cr0() {
        uint64_t value;
        asm volatile("mov %%cr0, %0" : "=r"(value));
        return value;
    }

    uint64_t read_cr4() {
        uint64_t value;
        asm volatile("mov %%cr4, %0" : "=r"(value));
        return value;
    }

    void disable() {
        asm volatile("cli" ::: "memory");
    }

    void enable() {
        asm volatile("sti" ::: "memory");
    }

}
