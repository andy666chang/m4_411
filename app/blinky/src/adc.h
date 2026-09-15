#ifndef ADC_H
#define ADC_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>

struct adc_dma_chunk {
    uint16_t *buffer;
    size_t length;
};

extern struct k_msgq adc_msgq;

void adc_sample_init(void);
void adc_sample_start(void);
void adc_sample_stop(void);

#endif /* ADC_H */
