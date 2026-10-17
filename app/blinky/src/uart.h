#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

int cdc_uart_init(void);
bool cdc_uart_host_ready(void);
void cdc_uart_clear_host_ready(void);
int send_adc_pack(const uint8_t *packet, size_t packet_length);

#endif /* UART_H */
