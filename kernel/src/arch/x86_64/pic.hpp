#pragma once

#include <cstdint>

namespace MyUnix::Arch::X86_64::PIC {

    bool initialize();

    void unmask_irq(uint8_t irq);
    void mask_irq(uint8_t irq);

    void send_eoi(uint8_t irq);

}
