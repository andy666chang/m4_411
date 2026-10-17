#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>

int cdc_uart_init(void);
/* The packet length field counts uint16_t ADC samples; payload bytes are little-endian. */
int cdc_uart_send_adc_packet(const uint16_t *samples, size_t sample_count,
                             uint8_t channel_count);

#endif /* UART_H */
