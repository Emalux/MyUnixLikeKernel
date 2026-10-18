#include <cstdint>
#include <cstddef>
#include <limine.h>

#include "mm/pmm.hpp"
#include "mm/vmm.hpp"
#include "mm/heap.hpp"
#include "arch/x86_64/idt.hpp"
#include "arch/x86_64/interrupts.hpp"
#include "arch/x86_64/pic.hpp"
#include "arch/x86_64/keyboard.hpp"


// Set the base revision to 6, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

namespace {

__attribute__((used, section(".limine_requests")))
volatile std::uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);

}

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

namespace {

__attribute__((used, section(".limine_requests")))
volatile limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request
memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request
hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID,
    .revision = 0,
    .response = nullptr
};

}

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .cpp file, as seen fit.

namespace {

__attribute__((used, section(".limine_requests_start")))
volatile std::uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
volatile std::uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

}

// Halt and catch fire function, defined below.
namespace {

void hcf();

}

// The following stubs are required by the Itanium C++ ABI (the one we use,
// regardless of the "Itanium" nomenclature).
// Like the memory functions in memory.cpp, these stubs can be moved to a different .cpp file,
// but should not be removed, unless you know what you are doing.
extern "C" {
    int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
    void __cxa_pure_virtual() { hcf(); }
    void __cxa_deleted_virtual() { hcf(); }
    void *__dso_handle;
    // Not thread safe: replace before statics can be initialised concurrently.
    int __cxa_guard_acquire(std::uint64_t *guard) { return *reinterpret_cast<std::uint8_t *>(guard) == 0; }
    void __cxa_guard_release(std::uint64_t *guard) { *reinterpret_cast<std::uint8_t *>(guard) = 1; }
}

// Extern declarations for global constructors array.
extern void (*__init_array[])();
extern void (*__init_array_end[])();

// Halt and catch fire function.
namespace {

void hcf() {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

// Scale an 8-bit colour channel value to the size the framebuffer gives the
// channel and move it into place within a pixel.
std::uint32_t fb_channel(std::uint8_t value, std::uint8_t mask_size, std::uint8_t mask_shift) {
    std::uint64_t max = (std::uint64_t{1} << mask_size) - 1;
    return static_cast<std::uint32_t>((value * max / 255) << mask_shift);
}

// Build a pixel from 8-bit red, green and blue values following the channel
// layout of the framebuffer.
std::uint32_t fb_pixel(limine_framebuffer *fb, std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
    return fb_channel(red, fb->red_mask_size, fb->red_mask_shift)
         | fb_channel(green, fb->green_mask_size, fb->green_mask_shift)
         | fb_channel(blue, fb->blue_mask_size, fb->blue_mask_shift);
}

// Print a nice pattern to a framebuffer as an example.
void fb_pattern(limine_framebuffer *fb) {
    volatile std::uint32_t *fb_ptr = static_cast<volatile std::uint32_t *>(fb->address);
    for (std::size_t y = 0; y < fb->height; y++) {
        for (std::size_t x = 0; x < fb->width; x++) {
            std::uint8_t nX = x * 255 / fb->width;
            std::uint8_t nY = y * 255 / fb->height;
            fb_ptr[y * (fb->pitch / 4) + x] = fb_pixel(fb, 0, nY, nX);
        }
    }
}

}
// MyUnix Serial COM1 Output (X86_64)
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static void serial_init() {
    constexpr uint16_t COM1 = 0x3F8;

    outb(COM1 + 1, 0x00); //disable interrupts
    outb(COM1 + 3, 0x80); //enable DLAB
    outb(COM1 + 0, 0x03); //set baud rate to 38400 (low byte)
    outb(COM1 + 1, 0x00); //set baud rate to
    outb(COM1 + 3, 0x03); //8 bits, no parity, one stop bit
    outb(COM1 + 2, 0xC7); //enable FIFO,
    outb(COM1 + 4, 0x0B); //IRQs enabled, RTS/DSR set
}

static void serial_write(const char* text) {
    constexpr uint16_t COM1 = 0x3F8;

    while (*text) {
        if (*text == '\n') {
            outb(COM1, '\r'); // Carriage return for new line
        }

        outb(COM1, static_cast<uint8_t>(*text));
        ++text;
    }
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
extern "C" void kmain() {

    // Just to see if the kernel is loading
    serial_init();
    serial_write("\n\n\n\n\n");
    serial_write("===============================\n");
    serial_write("         MyUnix Kernel\n");
    serial_write("===============================\n");
    serial_write("Architecture: x86_64\n");
    serial_write("Linguage: C++\n");
    serial_write("bootloader: Limine\n");
    serial_write("with love by Emalux <3\n");
    serial_write("kernel inicialized successfully!\n");
    serial_write("\n");

    // to see if the PMM is working and if it falls
    if (!MyUnix::Memory::initialize(
        memmap_request.response)) {
    serial_write("[PMM] failed at boot!\n");
} else {
    serial_write("[PMM] Initialized!\n");

    uint64_t page = MyUnix::Memory::allocate_page();

    if (page != 0) {
        serial_write("[PMM] Page allocated!\n");

        if (MyUnix::Memory::free_page(page)) {
            serial_write("[PMM] Page freed!\n");
        } else {
            serial_write("[PMM] Failed to free page!\n");
        }
    } else {
        serial_write("[PMM] No pages available!\n");
    }
}
    // to see if HHDM/VMM is working or it falls
    if (hhdm_request.response == nullptr) {
    serial_write("[VMM] HHDM unavailable!\n");
} else if (!MyUnix::Memory::VMM::initialize(
               hhdm_request.response->offset)) {
    serial_write("[VMM] Failed to initialize!\n");
} else {
    serial_write("[VMM] Initialized!\n");

    constexpr uint64_t TEST_VIRTUAL = 0x0000400000000000ULL;

    uint64_t page = MyUnix::Memory::allocate_page();

    if (page == 0) {
        serial_write("[VMM] No physical pages available!\n");
    } else if (!MyUnix::Memory::VMM::map_page(
                   TEST_VIRTUAL,
                   page,
                   MyUnix::Memory::VMM::Writable)) {
        serial_write("[VMM] Failed to map page!\n");
        MyUnix::Memory::free_page(page);
    } else {
        uint64_t translated = 0;

        if (MyUnix::Memory::VMM::translate(
                TEST_VIRTUAL, translated) &&
            translated == page) {
            serial_write("[VMM] Mapping confirmed!\n");
        } else {
            serial_write("[VMM] Translation failed!\n");
        }

        if (MyUnix::Memory::VMM::unmap_page(TEST_VIRTUAL)) {
            serial_write("[VMM] Mapping removed!\n");

            if (MyUnix::Memory::free_page(page)) {
                serial_write("[VMM] Physical page freed!\n");
            }
        }
    }
}

    // Kernel heap initialization

if (!MyUnix::Memory::Heap::initialize()) {
    serial_write("[HEAP] failed to initialize!\n");
} else {
    serial_write("[HEAP] Initialized!\n");

    auto* data = static_cast<uint8_t*>(
        MyUnix::Memory::Heap::allocate(64)
    );

    if (data == nullptr) {
        serial_write("[HEAP] failed to allocate!\n");
    } else {
        // Escreve na memória recém-alocada.
        for (size_t i = 0; i < 64; ++i) {
            data[i] = static_cast<uint8_t>(i);
        }

        bool valid = true;

        for (size_t i = 0; i < 64; ++i) {
            if (data[i] != static_cast<uint8_t>(i)) {
                valid = false;
                break;
            }
        }

        serial_write(valid
            ? "[HEAP] reading and writing OK!\n"
            : "[HEAP] failed to verify!\n");

        if (MyUnix::Memory::Heap::deallocate(data)) {
            serial_write("[HEAP] Memory freed!\n");
        } else {
            serial_write("[HEAP] failed to free memory!\n");
        }
    }
}

    // Initialize the IDT and set up exception handlers.
    if (!MyUnix::Arch::X86_64::IDT::initialize()) {
        
    serial_write("[IDT] Failed to initialize!\n");

    for (;;) {
        asm volatile("cli; hlt");
    }
}

serial_write("[IDT] Initialized!\n");

asm volatile("cli" ::: "memory");

if (!MyUnix::Arch::X86_64::PIC::initialize()) {
    serial_write("[PIC] Falha ao inicializar.\n");
    for (;;) {
        asm volatile("hlt");
    }
}

serial_write("[PIC] Inicializado. Vetores: 0x20-0x2F\n");

if (!MyUnix::Arch::X86_64::Keyboard::initialize()) {
    serial_write("[KEYBOARD] Falha ao inicializar PS/2.\n");
} else {
    serial_write("[KEYBOARD] PS/2 inicializado.\n");
    MyUnix::Arch::X86_64::PIC::unmask_irq(1);
}

serial_write("[BOOT] Habilitando interrupcoes\n");
asm volatile("sti" ::: "memory");
    for (;;) {
        asm volatile("hlt");
    }

    

    // Ensure the bootloader actually understands our base revision (see spec).
    if (LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision) == false) {
        hcf();
    }

    // Call global constructors.
    for (std::size_t i = 0; &__init_array[i] != __init_array_end; i++) {
        __init_array[i]();
    }

    // Ensure we got a framebuffer.
    if (framebuffer_request.response == nullptr
     || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Print the pattern to every framebuffer.
    for (std::uint64_t i = 0; i < framebuffer_request.response->framebuffer_count; i++) {
        limine_framebuffer *framebuffer = framebuffer_request.response->framebuffers[i];

        // Ensure the framebuffer has 32-bit RGB pixels, the only kind we handle.
        if (framebuffer->memory_model != LIMINE_FRAMEBUFFER_RGB || framebuffer->bpp != 32) {
            hcf();
        }

        fb_pattern(framebuffer);
    }

    // We're done, just hang...
    hcf();

    
}
