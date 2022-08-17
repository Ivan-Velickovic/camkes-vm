#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <autoconf.h>

#include <sel4vm/guest_irq_controller.h>
#include <sel4vm/boot.h>

#include <sel4vmmplatsupport/drivers/virtio_con.h>
#include <sel4vmmplatsupport/device.h>
#include <sel4vmmplatsupport/arch/vpci.h>

#include <virtio/virtio.h>
#include <virtio/virtio_plat.h>

typedef struct virtio_con_cookie {
    virtio_vsock_t *virtio_vsock;
} virtio_con_cookie_t;


virtio_vsock_t *virtio_vsock_init(vm_t *vm, vmm_pci_space_t *pci, vmm_io_port_list_t *io_ports) {
    int err;
    virtio_vsock_cookie_t *vsock_cookie;
    virtio_vsock_t *virtio_vsock;

    // @ivanv: do we need vsock cookie?
    vsock_cookie = (virtio_con_cookie_t *)calloc(1, sizeof(struct virtio_con_cookie));
    if (console_cookie == NULL) {
        ZF_LOGE("Failed to allocated virtio console cookie");
        return NULL;
    }

    // backend.console_data = (void *)console_cookie;
    ioport_range_t virtio_port_range = {0, 0, VIRTIO_IOPORT_SIZE};
    virtio_con = common_make_virtio_vsock(vm, pci, io_ports, virtio_port_range, IOPORT_FREE,
                                        VIRTIO_INTERRUPT_PIN, VIRTIO_CON_PLAT_INTERRUPT_LINE);
    console_cookie->virtio_con = virtio_con;
    console_cookie->vm = vm;
    err = vm_register_irq(vm->vcpus[BOOT_VCPU], VIRTIO_CON_PLAT_INTERRUPT_LINE, &virtio_vsock_ack, NULL);
    if (err) {
        ZF_LOGE("Failed to register console irq");
        return NULL;
    }
    return virtio_con;
}
