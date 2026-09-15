/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "adc.h"

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
    int ret;
    bool led_state = true;

    if (!gpio_is_ready_dt(&led)) {
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }

    while (1) {
        struct adc_dma_chunk msg;

        if (k_msgq_get(&adc_msgq, &msg, K_FOREVER) == 0) {
            printk("dma chunk: len=%zu ptr=%p\n", msg.length, (void *)msg.buffer);

            for (size_t i = 0; i < 4 && i < msg.length; i++) {
                printk("data[%zu]=%u\n", i, msg.buffer[i]);
            }
        }

        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0) {
            return 0;
        }

        // led_state = !led_state;
        // printf("LED state: %s\n", led_state ? "ON" : "OFF");
        // k_msleep(SLEEP_TIME_MS);
    }

    return 0;
}
