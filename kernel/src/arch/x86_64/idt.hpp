#pragma once

#include <cstdint>

namespace MyUnix::Arch::X86_64::IDT {

    constexpr uint16_t ENTRY_COUNT = 256;

    struct InterruptFrame {
        uint64_t r15;
        uint64_t r14;
        uint64_t r13;
        uint64_t r12;
        uint64_t r11;
        uint64_t r10;
        uint64_t r9;
        uint64_t r8;
        uint64_t rdi;
        uint64_t rsi;
        uint64_t rbp;
        uint64_t rdx;
        uint64_t rcx;
        uint64_t rbx;
        uint64_t rax;

        uint64_t vector;
        uint64_t error_code;
        uint64_t rip;
        uint64_t cs;
        uint64_t rflags;
    };

    bool initialize();
    uint64_t timer_ticks();

}

extern "C" void interrupt_dispatch(
    MyUnix::Arch::X86_64::IDT::InterruptFrame* frame
);

extern "C" [[noreturn]] void test_divide_by_zero();
extern "C" [[noreturn]] void test_invalid_opcode();