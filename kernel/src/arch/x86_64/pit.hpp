#pragma once

#include <cstdint>

namespace MyUnix::Arch::X86_64::PIT {
    void initialize(uint32_t frequency);
}
