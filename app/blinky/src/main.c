/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "adc.h"
#include "uart.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static uint16_t data_buf[DMA_HALF_SAMPLES];

int main(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led)) {
        return -ENODEV;
    }

    // ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    // if (ret < 0) {
    //     return 0;
    // }

    ret = cdc_uart_init();
    if (ret != 0) {
        printk("CDC UART initialization failed: %d\n", ret);
        return ret;
    }

    while (1) {
        struct adc_dma_chunk msg;

        if (k_msgq_get(&adc_msgq, &msg, K_FOREVER) == 0) {
            LOG_INF("Received ADC DMA chunk: buffer=%p, length=%zu", msg.buffer, msg.length);
            if (msg.length == 0U || msg.length > DMA_HALF_SAMPLES) {
                printk("Invalid ADC DMA chunk length: %zu\n", msg.length);
                continue;
            }

            memcpy(data_buf, msg.buffer, msg.length * sizeof(*data_buf));
            ret = cdc_uart_send_adc_packet(data_buf, msg.length, 1U);
            if (ret != 0) {
                printk("CDC UART packet transmission failed: %d\n", ret);
                return ret;
            }

            ret = gpio_pin_toggle_dt(&led);
            if (ret < 0) {
                printk("LED toggle failed: %d\n", ret);
                return ret;
            }

            LOG_INF("ADC DMA chunk sent over CDC UART: length=%zu", msg.length);
        }
    }

}



#include <zephyr/init.h>
#include <zephyr/drivers/pinctrl.h>

static int gpio_init(void) {

    printk("Config gpio table\n");

    PINCTRL_DT_DEFINE(DT_NODELABEL(gpio_init_cfg));
    const struct pinctrl_dev_config *gpio_init_pcfg =
        PINCTRL_DT_DEV_CONFIG_GET(DT_NODELABEL(gpio_init_cfg));
    int ret = pinctrl_apply_state(gpio_init_pcfg, PINCTRL_STATE_DEFAULT);
    if (ret < 0) {
        printk("Failed to apply pinctrl state: %d\n", ret);
        return ret;
    }

    return 0;
}

SYS_INIT(gpio_init, APPLICATION, 0);
