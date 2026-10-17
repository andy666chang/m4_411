#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>

#include "adc.h"
#include "uart.h"

#define ADC_PACKET_HEADER_SIZE  7U
#define ADC_PACKET_TRAILER_SIZE 4U
#define HOST_READY_BYTE         0xA5U

#define ADC_PACKET_MAX_SIZE \
    (ADC_PACKET_HEADER_SIZE + (DMA_HALF_SAMPLES * sizeof(uint16_t)) + \
     ADC_PACKET_TRAILER_SIZE)

static const uint8_t packet_start_magic[] = {'S', 'E', 'R', 'R'};
static const uint8_t packet_end_magic[] = {'I', 'A', 'L', 'L'};
static const struct device *const usb_uart_dev =
    DEVICE_DT_GET(DT_NODELABEL(my_cdc_uart));

static bool uart_initialized;
static uint8_t tx_packet[ADC_PACKET_MAX_SIZE];
static K_MUTEX_DEFINE(tx_mutex);
static K_SEM_DEFINE(host_ready, 0, 1);

static void uart_rx_callback(const struct device *dev, void *user_data)
{
    ARG_UNUSED(user_data);

    if (!uart_irq_update(dev) || !uart_irq_rx_ready(dev)) {
        return;
    }

    uint8_t byte;
    while (uart_fifo_read(dev, &byte, 1) == 1) {
        if (byte == HOST_READY_BYTE) {
            k_sem_give(&host_ready);
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

int cdc_uart_wait_host_ready(int32_t timeout_ms)
{
    if (!uart_initialized) {
        return -EACCES;
    }

    return k_sem_take(&host_ready, K_MSEC(timeout_ms));
}

int cdc_uart_send_adc_packet(const uint16_t *samples, size_t sample_count,
                             uint8_t channel_count)
{
    uint8_t *header = tx_packet;
    uint8_t *payload = &tx_packet[ADC_PACKET_HEADER_SIZE];
    uint8_t *trailer;
    size_t payload_bytes;
    size_t packet_bytes;
    int written;
    int ret;

    if (!uart_initialized) {
        return -EACCES;
    }
    if (samples == NULL || sample_count == 0U || channel_count == 0U ||
        sample_count > UINT16_MAX || sample_count % channel_count != 0U) {
        return -EINVAL;
    }

    ret = k_mutex_lock(&tx_mutex, K_FOREVER);
    if (ret != 0) {
        return ret;
    }

    /* Length is the number of uint16_t samples, as specified by the packet format. */
    payload_bytes = sample_count * sizeof(*samples);
    packet_bytes = ADC_PACKET_HEADER_SIZE + payload_bytes +
                   ADC_PACKET_TRAILER_SIZE;
    if (packet_bytes > sizeof(tx_packet) ||
        packet_bytes > CONFIG_USB_CDC_ACM_RINGBUF_SIZE) {
        k_mutex_unlock(&tx_mutex);
        return -EMSGSIZE;
    }

    memcpy(header, packet_start_magic, sizeof(packet_start_magic));
    header[4] = channel_count;
    header[5] = (uint8_t)(sample_count & 0xffU);
    header[6] = (uint8_t)((sample_count >> 8) & 0xffU);
    memcpy(payload, samples, payload_bytes);
    trailer = &tx_packet[ADC_PACKET_HEADER_SIZE + payload_bytes];
    memcpy(trailer, packet_end_magic, sizeof(packet_end_magic));

    written = uart_fifo_fill(usb_uart_dev, tx_packet, (int)packet_bytes);
    if (written != (int)packet_bytes) {
        ret = written < 0 ? written : -ENOSPC;
    } else {
        ret = 0;
    }

    k_mutex_unlock(&tx_mutex);
    return ret;
}