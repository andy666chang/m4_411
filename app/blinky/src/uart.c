#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/usb/usb_device.h>

#include "uart.h"

#define HOST_READY_BYTE 0xA5U

static const struct device *const usb_uart_dev =
    DEVICE_DT_GET(DT_NODELABEL(my_cdc_uart));

static bool uart_initialized;
static atomic_t host_ready;

static void uart_rx_callback(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) {
        return;
    }

    uint8_t byte;
    while (uart_fifo_read(dev, &byte, 1) == 1) {
        if (byte == HOST_READY_BYTE) {
            atomic_set(&host_ready, 1);
        }
    }
}

int cdc_uart_init(void)
{
    int ret;

    if (!device_is_ready(usb_uart_dev)) {
        return -ENODEV;
    }

    ret = usb_enable(NULL);
    if (ret != 0) {
        return ret;
    }

    uart_irq_callback_set(usb_uart_dev, uart_rx_callback);
    uart_irq_rx_enable(usb_uart_dev);
    uart_initialized = true;
    return 0;
}

bool cdc_uart_host_ready(void)
{
    return atomic_get(&host_ready) != 0;
}

void cdc_uart_clear_host_ready(void)
{
    atomic_clear(&host_ready);
}

int send_adc_pack(const uint8_t *packet, size_t packet_length)
{
    int written;

    if (!uart_initialized) {
        return -EACCES;
    }
    if (packet == NULL || packet_length == 0U ||
        packet_length > CONFIG_USB_CDC_ACM_RINGBUF_SIZE ||
        packet_length > INT_MAX) {
        return -EINVAL;
    }

    written = uart_fifo_fill(usb_uart_dev, packet, (int)packet_length);
    if (written != (int)packet_length) {
        return written < 0 ? written : -ENOSPC;
    }

    return 0;
}
