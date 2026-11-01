#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>

// static const struct device *const usb_uart_dev =
//     DEVICE_DT_GET(DT_NODELABEL(my_cdc_uart));

static int cdc_init(void) {
    int ret;

    // if (!device_is_ready(usb_uart_dev)) {
    //     return -ENODEV;
    // }

    ret = usb_enable(NULL);
    if (ret != 0) {
        return ret;
    }

    return 0;
}

SYS_INIT(cdc_init, APPLICATION, 0);
