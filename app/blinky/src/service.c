#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "adc.h"
#include "service.h"
#include "uart.h"

LOG_MODULE_REGISTER(service, LOG_LEVEL_INF);

#define PACKET_START_MAGIC "SERR"
#define PACKET_END_MAGIC   "IALL"
#define PACKET_HEADER_SIZE 7U
#define PACKET_TRAILER_SIZE 4U
#define SERVICE_STACK_SIZE 2048
#define SERVICE_PRIORITY 5

#define LED0_NODE DT_ALIAS(led0)

enum service_state {
    SERVICE_WAIT,
    SERVICE_SEND,
};

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static K_THREAD_STACK_DEFINE(service_stack, SERVICE_STACK_SIZE);
static struct k_thread service_thread;
static uint8_t packet[PACKET_HEADER_SIZE +
                      (DMA_HALF_SAMPLES * sizeof(uint16_t)) +
                      PACKET_TRAILER_SIZE];
static bool service_started;

static void service_thread_fn(void *arg1, void *arg2, void *arg3)
{
    ARG_UNUSED(arg1);
    ARG_UNUSED(arg2);
    ARG_UNUSED(arg3);

    enum service_state state = SERVICE_WAIT;
    struct adc_dma_chunk msg;
    bool host_ready = false;

    while (true) {
        int ret = k_msgq_get(&adc_msgq, &msg, K_FOREVER);
        if (ret != 0) {
            LOG_ERR("ADC message queue receive failed: %d", ret);
            continue;
        }

        bool send_current_msg = false;
        switch (state) {
        case SERVICE_WAIT:
            host_ready = cdc_uart_host_ready();

            if (!host_ready) {
                LOG_INF("WAIT: dropping ADC data; host is not ready");
                break;
            }

            state = SERVICE_SEND;
            LOG_INF("WAIT -> SEND: host ready");
            send_current_msg = true;
            break;

        case SERVICE_SEND:
            host_ready = cdc_uart_host_ready();

            if (!host_ready) {
                LOG_WRN("SEND -> WAIT: host not ready");
                state = SERVICE_WAIT;
                break;
            }

            send_current_msg = true;
            break;
        }

        if (!send_current_msg) {
            continue;
        }

        if (msg.buffer == NULL || msg.length == 0U ||
            msg.length > DMA_HALF_SAMPLES || msg.length > UINT16_MAX) {
            LOG_ERR("Invalid ADC chunk: buffer=%p length=%zu",
                    msg.buffer, msg.length);
            continue;
        }

        memcpy(packet, PACKET_START_MAGIC, 4U);
        packet[4] = 1U;
        packet[5] = (uint8_t)(msg.length & 0xffU);
        packet[6] = (uint8_t)((msg.length >> 8) & 0xffU);
        memcpy(&packet[PACKET_HEADER_SIZE], msg.buffer,
               msg.length * sizeof(*msg.buffer));
        memcpy(&packet[PACKET_HEADER_SIZE +
                       (msg.length * sizeof(*msg.buffer))],
               PACKET_END_MAGIC, PACKET_TRAILER_SIZE);

        size_t packet_length = PACKET_HEADER_SIZE +
                               (msg.length * sizeof(*msg.buffer)) +
                               PACKET_TRAILER_SIZE;
        ret = send_adc_pack(packet, packet_length);
        if (ret != 0) {
            LOG_ERR("CDC packet send failed: %d; returning to WAIT", ret);
            host_ready = false;
            cdc_uart_clear_host_ready();
            state = SERVICE_WAIT;
            continue;
        }

        ret = gpio_pin_toggle_dt(&led);
        if (ret != 0) {
            LOG_ERR("LED toggle failed: %d", ret);
        }
    }
}

int service_start(void)
{
    int ret;

    if (service_started) {
        return -EALREADY;
    }

    if (!gpio_is_ready_dt(&led)) {
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret != 0) {
        LOG_ERR("LED configuration failed: %d", ret);
        return ret;
    }

    k_thread_create(&service_thread, service_stack,
                    K_THREAD_STACK_SIZEOF(service_stack),
                    service_thread_fn, NULL, NULL, NULL,
                    SERVICE_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&service_thread, "adc_service");
    service_started = true;
    return 0;
}
