// @ivanv: I don't think this should be in this directory.
// It's odd that virtio_net.c is here, but virtio_con.c is in the VM_Arm dir?

// @ivanv: some of these will need to be rmeoved
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <autoconf.h>

#include <sel4platsupport/arch/io.h>
#include <sel4utils/vspace.h>
#include <sel4utils/iommu_dma.h>
#include <simple/simple_helpers.h>
#include <vka/capops.h>
#include <utils/util.h>

#include <camkes.h>
#include <camkes/dataport.h>

#include <ethdrivers/virtio/virtio_pci.h>
#include <ethdrivers/virtio/virtio_vsock.h>
#include <ethdrivers/virtio/virtio_ring.h>

#include <sel4vm/guest_vm.h>
#include <sel4vm/guest_memory.h>
#include <sel4vm/guest_irq_controller.h>
#include <sel4vm/boot.h>

#include <sel4vmmplatsupport/drivers/pci.h>
#include <sel4vmmplatsupport/drivers/virtio_pci_emul.h>
#include <sel4vmmplatsupport/drivers/virtio_vsock.h>

#include "vm.h"
#include "virtio_net.h"

/* @ivanv: Why is this 8? Well, because virtIO blk uses 7 and virtIO net uses 6. */
#define VIRTIO_VSOCK_IRQ 8

void make_virtio_vsock(vm_t *vm, vmm_pci_space_t *pci, vmm_io_port_list_t *io_ports)
{
    int err;
    // virtio_vsock_cookie_t *vsock_cookie;
    virtio_vsock_t *virtio_vsock;

    // @ivanv: do we need vsock cookie?
    // vsock_cookie = (virtio_con_cookie_t *)calloc(1, sizeof(struct virtio_con_cookie));
    // if (console_cookie == NULL) {
    //     ZF_LOGE("Failed to allocated virtio console cookie");
    //     return NULL;
    // }

    // backend.raw_handleIRQ = emul_raw_handle_irq;

    // backend.console_data = (void *)console_cookie;
    // @ivanv: not sure what this all means, with IOPORT stuff
    // @ivanv: this might be wrong
    ioport_range_t virtio_port_range = {0, 0, VIRTIO_IOPORT_SIZE};
    // @ivanv: do we want IOPORT_FREE or IOPORT_ADDR?
    virtio_vsock = common_make_virtio_vsock(vm, pci, io_ports, virtio_port_range, IOPORT_FREE,
                                        VIRTIO_VSOCK_IRQ, VIRTIO_VSOCK_IRQ);
    // console_cookie->virtio_con = virtio_con;
    // console_cookie->vm = vm;
    assert(virtio_vsock);
}
