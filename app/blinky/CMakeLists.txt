# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blinky)

target_sources(app PRIVATE src/main.c src/adc.c)

set(ELF_FILE ${PROJECT_BINARY_DIR}/zephyr/zephyr.elf)

add_custom_target(dap-flash
    COMMAND openocd -f interface/cmsis-dap.cfg -f target/stm32f4x.cfg -c "program ${ELF_FILE} verify reset exit"
    DEPENDS ${ELF_FILE}
    COMMENT "正在透過 dap-link 燒錄 ${ELF_FILE} ..."
    USES_TERMINAL
)
