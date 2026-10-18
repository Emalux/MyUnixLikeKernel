
#include "keyboard.hpp"

namespace MyUnix::Arch::X86_64::Keyboard {

namespace {

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

bool wait_input_clear() {
    for (uint32_t i = 0; i < 100000; ++i) {
        if ((inb(0x64) & 0x02) == 0) {
            return true;
        }
    }
    return false;
}

bool wait_output_full() {
    for (uint32_t i = 0; i < 100000; ++i) {
        if (inb(0x64) & 0x01) {
            return true;
        }
    }
    return false;
}

bool left_shift = false;
bool right_shift = false;
bool caps_lock = false;

constexpr char normal_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*',
    0, ' ', 0
};

constexpr char shifted_map[128] = {
    0,  27, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*',
    0, ' ', 0
};

}

bool initialize() {
    // Ativa a primeira porta PS/2, se o controlador a suportar.
    if (!wait_input_clear()) {
        return false;
    }

    outb(0x64, 0xAE);

    // Lê o byte de configuração do controlador.
    if (!wait_input_clear()) {
        return false;
    }

    outb(0x64, 0x20);

    if (!wait_output_full()) {
        return false;
    }

    uint8_t config = inb(0x60);

    // Habilita IRQ1 e garante que a primeira porta não está desativada.
    config |= 0x01;
    config &= static_cast<uint8_t>(~0x10);

    if (!wait_input_clear()) {
        return false;
    }

    outb(0x64, 0x60);

    if (!wait_input_clear()) {
        return false;
    }

    outb(0x60, config);

    return true;
}

char handle_scancode(uint8_t scancode) {
    bool shift_pressed = left_shift || right_shift;
    // Scancodes de soltura no conjunto 1.
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
        return 0;
    }

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return 0;
    }

    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
        return 0;
    }

    // Ignora eventos de soltura e códigos estendidos nesta versão.
    if (scancode & 0x80) {
        return 0;
    }

    if (scancode >= 128) {
        return 0;
    }

    char c = shift_pressed
        ? shifted_map[scancode]
        : normal_map[scancode];

    if (c >= 'a' && c <= 'z' && (shift_pressed != caps_lock)) {
        c = static_cast<char>(c - 'a' + 'A');
    }

    return c;
}

}
