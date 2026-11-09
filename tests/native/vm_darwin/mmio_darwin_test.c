#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "native/core/mmio.h"

#include <assert.h>
#include <string.h>
#include <ucontext.h>

static uint32_t stored_value;
static unsigned store_count;
static uint32_t stored_pa;
static uint32_t stored_len;

static uint64_t mmio_read(void *opaque, uint32_t off, unsigned size)
{
    assert(opaque == NULL);
    assert(off == 4);
    assert(size == 4);
    return 0x12345678;
}

static void mmio_write(void *opaque, uint32_t off, uint64_t value, unsigned size)
{
    assert(opaque == NULL);
    assert(off == 4);
    assert(size == 4);
    stored_value = (uint32_t)value;
    store_count++;
}

static void ram_stored(uint32_t pa, uint32_t len)
{
    stored_pa = pa;
    stored_len = len;
}

int main(void)
{
    const uint32_t load_w0_x1[] = {0xB9400020};
    const uint32_t store_w0_x1[] = {0xB9000020};
    const uint32_t load_q0_x1[] = {0x3DC00020};
    const uint32_t store_q0_x1[] = {0x3D800020};
    const uintptr_t guest_base = 0x10000000000ull;
    uint32_t pte[2] = {0, 0x1000 | NC_PTE_VALID};
    uint8_t phys[0x2000] = {0};
    NcVm vm = {
        .base = (uint8_t *)guest_base,
        .ram_size = sizeof(phys),
        .phys = phys,
        .pte = pte,
    };
    NcMmioDev dev = {
        .name = "test",
        .base = 0xFD000000,
        .size = 0x1000,
        .read = mmio_read,
        .write = mmio_write,
    };
    nc_mmio_register(&dev);

    struct __darwin_mcontext64 mc = {0};
    ucontext_t uc = {0};
    uc.uc_mcontext = &mc;
    siginfo_t info = {0};
    info.si_addr = (void *)(guest_base + dev.base + 4);

    mc.__ss.__pc = (uintptr_t)load_w0_x1;
    mc.__ss.__x[1] = guest_base + dev.base + 4;
    assert(nc_mmio_fault(&vm, &info, &uc));
    assert(mc.__ss.__x[0] == 0x12345678);
    assert(mc.__ss.__pc == (uintptr_t)load_w0_x1 + sizeof(load_w0_x1[0]));

    mc.__ss.__pc = (uintptr_t)store_w0_x1;
    mc.__ss.__x[0] = 0xA5A55A5A;
    mc.__ss.__x[1] = guest_base + dev.base + 4;
    assert(nc_mmio_fault(&vm, &info, &uc));
    assert(store_count == 1);
    assert(stored_value == 0xA5A55A5A);
    assert(mc.__ss.__pc == (uintptr_t)store_w0_x1 + sizeof(store_w0_x1[0]));

    info.si_addr = (void *)(guest_base + 0x1000);
    phys[0x1000] = 0x78;
    phys[0x1001] = 0x56;
    phys[0x1002] = 0x34;
    phys[0x1003] = 0x12;
    mc.__ss.__pc = (uintptr_t)load_w0_x1;
    mc.__ss.__x[1] = guest_base + 0x1000;
    assert(nc_ram_emulate(&vm, &info, &uc, ram_stored));
    assert(mc.__ss.__x[0] == 0x12345678);
    assert(mc.__ss.__pc == (uintptr_t)load_w0_x1 + sizeof(load_w0_x1[0]));

    mc.__ss.__pc = (uintptr_t)store_w0_x1;
    mc.__ss.__x[0] = 0xDEADBEEF;
    mc.__ss.__x[1] = guest_base + 0x1000;
    assert(nc_ram_emulate(&vm, &info, &uc, ram_stored));
    assert(phys[0x1000] == 0xEF && phys[0x1001] == 0xBE && phys[0x1002] == 0xAD && phys[0x1003] == 0xDE);
    assert(stored_pa == 0x1000 && stored_len == 4);

    for (unsigned i = 0; i < 16; i++) {
        phys[0x1000 + i] = (uint8_t)(i + 1);
    }
    mc.__ss.__pc = (uintptr_t)load_q0_x1;
    mc.__ss.__x[1] = guest_base + 0x1000;
    assert(nc_ram_emulate(&vm, &info, &uc, ram_stored));
    assert(memcmp(&mc.__ns.__v[0], phys + 0x1000, 16) == 0);

    mc.__ss.__pc = (uintptr_t)store_q0_x1;
    for (unsigned i = 0; i < 16; i++) {
        ((uint8_t *)&mc.__ns.__v[0])[i] = (uint8_t)(0xF0 + i);
    }
    mc.__ss.__x[1] = guest_base + 0x1000;
    assert(nc_ram_emulate(&vm, &info, &uc, ram_stored));
    for (unsigned i = 0; i < 16; i++) {
        assert(phys[0x1000 + i] == (uint8_t)(0xF0 + i));
    }
    assert(stored_pa == 0x1000 && stored_len == 16);
    return 0;
}
