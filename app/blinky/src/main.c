/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/kernel.h>

#include "service.h"
#include "uart.h"

int main(void)
{
    int ret;

    ret = cdc_uart_init();
    if (ret != 0) {
        printk("CDC UART initialization failed: %d\n", ret);
        return ret;
    }

    ret = service_start();
    if (ret != 0) {
        printk("ADC service start failed: %d\n", ret);
        return ret;
    }

    return 0;
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
