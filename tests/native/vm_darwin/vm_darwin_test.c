#include "native/core/vm.h"

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>

int hle_uma_ram_fd(size_t size)
{
    (void)size;
    return -1;
}

void nc_log(const char *tag, const char *fmt, ...)
{
    (void)tag;
    (void)fmt;
}

int main(void)
{
    NcVm vm;
    if (!nc_vm_init(&vm, NC_RAM_RETAIL)) {
        fputs("guest RAM initialization failed\n", stderr);
        return 1;
    }

    struct stat backing;
    const int fd_flags = fcntl(vm.ram_fd, F_GETFD);
    if (fd_flags < 0 || !(fd_flags & FD_CLOEXEC) || fstat(vm.ram_fd, &backing) != 0 ||
        backing.st_nlink != 0) {
        fputs("guest RAM backing fd is not unlinked and close-on-exec\n", stderr);
        return 2;
    }

    vm.phys[0] = 0x5a;
    if (vm.ram[0] != 0x5a || !nc_vm_map(&vm, 0x20000, 0x10000, NC_PAGE)) {
        fputs("guest RAM views are not shared\n", stderr);
        return 3;
    }
    vm.phys[0x10000] = 0xa5;
    if (vm.base[0x20000] != 0xa5 || !nc_vm_unmap(&vm, 0x20000, NC_PAGE) ||
        nc_vm_phys(&vm, 0x20000) != UINT32_MAX) {
        fputs("guest VA mapping or unmapping failed\n", stderr);
        return 4;
    }

    puts("Darwin guest RAM backing and mappings passed");
    return 0;
}
