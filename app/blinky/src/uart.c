#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/usb/usb_device.h>

#include "uart.h"

#define ADC_PACKET_HEADER_SIZE  7U

static const uint8_t packet_start_magic[] = {'S', 'E', 'R', 'R'};
static const uint8_t packet_end_magic[] = {'I', 'A', 'L', 'L'};
static const struct device *const usb_uart_dev =
    DEVICE_DT_GET(DT_NODELABEL(my_cdc_uart));

static bool uart_initialized;

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

    uart_initialized = true;
    return 0;
}

int cdc_uart_send_adc_packet(const uint16_t *samples, size_t sample_count,
                             uint8_t channel_count)
{
    uint8_t header[ADC_PACKET_HEADER_SIZE] = {
        0U, 0U, 0U, 0U, channel_count,
    };
    const uint8_t *payload = (const uint8_t *)samples;
    size_t payload_bytes;

    if (!uart_initialized) {
        return -EACCES;
    }
    if (samples == NULL || sample_count == 0U || channel_count == 0U ||
        sample_count > UINT16_MAX || sample_count % channel_count != 0U) {
        return -EINVAL;
    }

    /* Length is the number of uint16_t samples, as specified by the packet format. */
    memcpy(header, packet_start_magic, sizeof(packet_start_magic));
    header[5] = (uint8_t)(sample_count & 0xffU);
    header[6] = (uint8_t)((sample_count >> 8) & 0xffU);
    payload_bytes = sample_count * sizeof(*samples);

    for (size_t i = 0; i < sizeof(header); ++i) {
        uart_poll_out(usb_uart_dev, header[i]);
    }
    for (size_t i = 0; i < payload_bytes; ++i) {
        uart_poll_out(usb_uart_dev, payload[i]);
    }
    for (size_t i = 0; i < sizeof(packet_end_magic); ++i) {
        uart_poll_out(usb_uart_dev, packet_end_magic[i]);
    }

    return 0;
}