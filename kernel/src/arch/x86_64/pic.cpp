
#include "pic.hpp"

namespace MyUnix::Arch::X86_64::PIC {

namespace {

    constexpr uint16_t PIC1_COMMAND = 0x20;
    constexpr uint16_t PIC1_DATA    = 0x21;

    constexpr uint16_t PIC2_COMMAND = 0xA0;
    constexpr uint16_t PIC2_DATA    = 0xA1;

    constexpr uint8_t ICW1_INIT = 0x10;
    constexpr uint8_t ICW1_ICW4 = 0x01;
    constexpr uint8_t ICW4_8086 = 0x01;

    constexpr uint8_t MASTER_OFFSET = 0x20;
    constexpr uint8_t SLAVE_OFFSET  = 0x28;

    inline void outb(uint16_t port, uint8_t value)
    {
        asm volatile(
            "outb %0, %1"
            :
            : "a"(value), "Nd"(port)
        );
    }

    inline uint8_t inb(uint16_t port)
    {
        uint8_t value;

        asm volatile(
            "inb %1, %0"
            : "=a"(value)
            : "Nd"(port)
        );

        return value;
    }

    void io_wait()
    {
        outb(0x80, 0);
    }

}

bool initialize()
{
    // Salva as máscaras atuais.
    uint8_t master_mask = inb(PIC1_DATA);
    uint8_t slave_mask  = inb(PIC2_DATA);

    // Inicia a sequência de inicialização.
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    // Remapeia os vetores para não conflitar com as exceções da CPU.
    outb(PIC1_DATA, MASTER_OFFSET);
    io_wait();

    outb(PIC2_DATA, SLAVE_OFFSET);
    io_wait();

    // Informa a ligação entre os controladores.
    outb(PIC1_DATA, 0x04); // IRQ2 conecta o PIC slave.
    io_wait();

    outb(PIC2_DATA, 0x02);
    io_wait();

    // Define o modo compatível com x86.
    outb(PIC1_DATA, ICW4_8086);
    io_wait();

    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    // Inicialmente, mantém todas as IRQs mascaradas.
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    (void)master_mask;
    (void)slave_mask;

    return true;
}

void unmask_irq(uint8_t irq)
{
    if (irq >= 16)
        return;

    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = PIC1_DATA;
        mask = static_cast<uint8_t>(~(1u << irq));

        uint8_t current = inb(port);
        outb(port, current & ~(1u << irq));
    } else {
        // O PIC slave depende da IRQ2 do PIC master.
        uint8_t master = inb(PIC1_DATA);
        outb(PIC1_DATA, master & ~(1u << 2));

        port = PIC2_DATA;
        mask = static_cast<uint8_t>(~(1u << (irq - 8)));

        uint8_t current = inb(port);
        outb(port, current & mask);
    }
}

void mask_irq(uint8_t irq)
{
    if (irq >= 16)
        return;

    if (irq < 8) {
        uint8_t current = inb(PIC1_DATA);
        outb(PIC1_DATA, current | (1u << irq));
    } else {
        uint8_t current = inb(PIC2_DATA);
        outb(PIC2_DATA, current | (1u << (irq - 8)));
    }
}

void send_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_COMMAND, 0x20);

    outb(PIC1_COMMAND, 0x20);
}

}
