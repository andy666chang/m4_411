#ifndef ADC_H
#define ADC_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>

/* Each DMA half contains 1000 samples, about one second at 1 kHz. */
#define DMA_BUF_SIZE_BYTES 4000U
#define DMA_BUF_SAMPLES (DMA_BUF_SIZE_BYTES / sizeof(uint16_t))
#define DMA_HALF_SAMPLES (DMA_BUF_SAMPLES / 2U)

struct adc_dma_chunk {
    uint16_t *buffer;
    size_t length;
};

extern struct k_msgq adc_msgq;

void adc_sample_init(void);
void adc_sample_start(void);
void adc_sample_stop(void);

#endif /* ADC_H */
