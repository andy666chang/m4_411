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

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
// extern struct k_msgq adc_msgq;

int main(void)
{
	int ret;
	bool led_state = true;
	uint16_t *ready_buf_ptr;

	if (!gpio_is_ready_dt(&led)) {
		return 0;
	}

	// adc_sample_init();
	// adc_sample_start();

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return 0;
	}

	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		printf("LED state: %s\n", led_state ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);

		/* 等待 DMA 中斷推入已填滿的半個 Buffer 指標 (512 samples / 1KB) */
        // if (k_msgq_get(&adc_msgq, &ready_buf_ptr, K_FOREVER) == 0) {
            
        //     // 處理 512 個點 (例如計算 RMS, FFT 或發送到 Queue)
        //     for (int i = 0; i < 512; i++) {
        //         uint16_t sample_val = ready_buf_ptr[i];
        //         (void)sample_val;
		// 		printf("LED state: %s\n", led_state ? "ON" : "OFF");
        //     }
        // }
	}
	return 0;
}
