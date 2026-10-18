#include "idt.hpp"
#include "pic.hpp"
#include "keyboard.hpp"

#include <cstddef>
#include <cstdint>

namespace {

constexpr uint16_t COM1 = 0x3F8;

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init() {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_char(char c) {
    for (unsigned i = 0; i < 1000000; ++i) {
        if (inb(COM1 + 5) & 0x20) {
            outb(COM1, static_cast<uint8_t>(c));
            return;
        }
    }
}

void serial_text(const char* text) {
    while (*text) serial_char(*text++);
}

void serial_hex(uint64_t value) {
    static constexpr char digits[] = "0123456789ABCDEF";
    serial_text("0x");

    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_char(digits[(value >> shift) & 0xF]);
    }
}

}


namespace MyUnix::Arch::X86_64::IDT {


static const char* const exception_names[32] = {
    "Division Error",
    "Debug",
    "NMI",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};


    struct __attribute__((packed)) IDTEntry {
        uint16_t offset_low;
        uint16_t selector;
        uint8_t ist;
        uint8_t attributes;
        uint16_t offset_mid;
        uint32_t offset_high;
        uint32_t reserved;
    };

    struct __attribute__((packed)) IDTRegister {
        uint16_t limit;
        uint64_t base;
    };

    static IDTEntry idt[ENTRY_COUNT]{};

    extern "C" void isr0();
    extern "C" void isr1();
    extern "C" void isr2();
    extern "C" void isr3();
    extern "C" void isr4();
    extern "C" void isr5();
    extern "C" void isr6();
    extern "C" void isr7();
    extern "C" void isr8();
    extern "C" void isr9();
    extern "C" void isr10();
    extern "C" void isr11();
    extern "C" void isr12();
    extern "C" void isr13();
    extern "C" void isr14();
    extern "C" void isr15();
    extern "C" void isr16();
    extern "C" void isr17();
    extern "C" void isr18();
    extern "C" void isr19();
    extern "C" void isr20();
    extern "C" void isr21();
    extern "C" void isr22();
    extern "C" void isr23();
    extern "C" void isr24();
    extern "C" void isr25();
    extern "C" void isr26();
    extern "C" void isr27();
    extern "C" void isr28();
    extern "C" void isr29();
    extern "C" void isr30();
    extern "C" void isr31();
    extern "C" void isr32();
    extern "C" void isr33();

    static void* const exception_handlers[32] = {
        reinterpret_cast<void*>(isr0),
        reinterpret_cast<void*>(isr1),
        reinterpret_cast<void*>(isr2),
        reinterpret_cast<void*>(isr3),
        reinterpret_cast<void*>(isr4),
        reinterpret_cast<void*>(isr5),
        reinterpret_cast<void*>(isr6),
        reinterpret_cast<void*>(isr7),
        reinterpret_cast<void*>(isr8),
        reinterpret_cast<void*>(isr9),
        reinterpret_cast<void*>(isr10),
        reinterpret_cast<void*>(isr11),
        reinterpret_cast<void*>(isr12),
        reinterpret_cast<void*>(isr13),
        reinterpret_cast<void*>(isr14),
        reinterpret_cast<void*>(isr15),
        reinterpret_cast<void*>(isr16),
        reinterpret_cast<void*>(isr17),
        reinterpret_cast<void*>(isr18),
        reinterpret_cast<void*>(isr19),
        reinterpret_cast<void*>(isr20),
        reinterpret_cast<void*>(isr21),
        reinterpret_cast<void*>(isr22),
        reinterpret_cast<void*>(isr23),
        reinterpret_cast<void*>(isr24),
        reinterpret_cast<void*>(isr25),
        reinterpret_cast<void*>(isr26),
        reinterpret_cast<void*>(isr27),
        reinterpret_cast<void*>(isr28),
        reinterpret_cast<void*>(isr29),
        reinterpret_cast<void*>(isr30),
        reinterpret_cast<void*>(isr31)
    };

    static void set_gate(
        uint8_t vector,
        void* handler,
        uint16_t selector
    ) {
        const uint64_t address =
            reinterpret_cast<uint64_t>(handler);

        idt[vector].offset_low =
            static_cast<uint16_t>(address & 0xFFFF);

        idt[vector].selector = selector;
        idt[vector].ist = 0;
        idt[vector].attributes = 0x8E;

        idt[vector].offset_mid =
            static_cast<uint16_t>((address >> 16) & 0xFFFF);

        idt[vector].offset_high =
            static_cast<uint32_t>((address >> 32) & 0xFFFFFFFF);

        idt[vector].reserved = 0;
    }

    bool initialize() {

        serial_init();

        uint16_t code_selector;

        asm volatile(
            "mov %%cs, %0"
            : "=r"(code_selector)
        );

        for (uint8_t i = 0; i < 32; ++i) {
            set_gate(i, exception_handlers[i], code_selector);
        }

        set_gate(32, reinterpret_cast<void*>(isr32), code_selector);
        set_gate(33, reinterpret_cast<void*>(isr33), code_selector);

        IDTRegister idtr{
            static_cast<uint16_t>(sizeof(idt) - 1),
            reinterpret_cast<uint64_t>(&idt[0])
        };

        asm volatile(
            "lidt %0"
            :
            : "m"(idtr)
            : "memory"
        );

        return true;
    }

    uint64_t timer_ticks();

}


extern "C" void interrupt_dispatch(
    MyUnix::Arch::X86_64::IDT::InterruptFrame* frame
) {
if (frame->vector == 32) {
    MyUnix::Arch::X86_64::PIC::send_eoi(0);
    return;
}

if (frame->vector == 33) {
    // IRQ1: teclado PS/2.
    const uint8_t scancode = inb(0x60);

   serial_text("[IRQ1] Tecla recebida: ");
    serial_hex(scancode);
    serial_text("\n");

    MyUnix::Arch::X86_64::PIC::send_eoi(1);
    return;
}

    asm volatile("cli");

    serial_text("\n\n[EXCEPTION] CPU Exception!\n");

    if (frame->vector < 32) {
        serial_text("Name: ");
        serial_text(
            MyUnix::Arch::X86_64::IDT::
                exception_names[frame->vector]
        );
        serial_text("\n");
    } else {
        serial_text("Name: Unknown Interrupt\n");
    }

    serial_text("Vector: ");
    serial_hex(frame->vector);

    serial_text("\nRIP: ");
    serial_hex(frame->rip);

    serial_text("\nError Code: ");
    serial_hex(frame->error_code);

    if (frame->vector == 14) {
        uint64_t cr2;

        asm volatile("mov %%cr2, %0" : "=r"(cr2));

        serial_text("\nCR2 (faulting address): ");
        serial_hex(cr2);
    }

    serial_text("\n[EXCEPTION] Kernel stopped.\n");

    for (;;) {
        asm volatile("cli; hlt");
    }
}




extern "C" [[noreturn]] void test_divide_by_zero() {
    asm volatile(
        "xor %%rax, %%rax\n\t"
        "xor %%rdx, %%rdx\n\t"
        "xor %%rcx, %%rcx\n\t"
        "divq %%rcx"
        :
        :
        : "rax", "rdx", "rcx"
    );

    for (;;) {
        asm volatile("cli; hlt");
    }
}

extern "C" [[noreturn]] void test_invalid_opcode() {
    asm volatile("ud2");

    for (;;) {
        asm volatile("cli; hlt");
    }
}



