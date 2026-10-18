#pragma once

#include <cstdint>

namespace MyUnix::Arch::X86_64::Keyboard {

    bool initialize();
    char handle_scancode(uint8_t scancode);

}
