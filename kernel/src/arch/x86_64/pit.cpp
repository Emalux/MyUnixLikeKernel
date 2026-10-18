#include "pit.hpp"

namespace MyUnix::Arch::X86_64::PIT {

namespace {
inline void outb(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
}

void initialize(uint32_t frequency)
{
    if (frequency == 0)
        return;

    constexpr uint32_t PIT_INPUT_HZ = 1193182;
    uint32_t divisor = PIT_INPUT_HZ / frequency;

    if (divisor == 0)
        divisor = 1;
    if (divisor > 65535)
        divisor = 65535;

    outb(0x43, 0x36); // Canal 0, low/high byte, modo 3
    outb(0x40, static_cast<uint8_t>(divisor & 0xFF));
    outb(0x40, static_cast<uint8_t>((divisor >> 8) & 0xFF));
}

}
