#pragma once

#include <cstdint>

namespace MyUnix::Arch::X86_64::Interrupts {

    uint64_t read_rflags();
    uint64_t read_cr0();
    uint64_t read_cr4();

    void disable();
    void enable();

}
