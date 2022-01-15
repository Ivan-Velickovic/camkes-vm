#pragma once

#include <sel4arm-vmm/vm.h>

#define LINUX_RAM_BASE    0x40000000
#define LINUX_RAM_PADDR_BASE LINUX_RAM_BASE
#define LINUX_RAM_SIZE    0x80000000
static const int linux_pt_irqs[] = {
    27, // VTCNT
};
