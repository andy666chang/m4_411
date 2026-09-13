#include <soc.h>
#include <stm32f4xx.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

/* DMA Buffer 設定: 2KB 大小，存放 1024 個 16-bit (Half-Word) 採樣點 */
#define DMA_BUF_SIZE_BYTES 2048
#define DMA_BUF_SAMPLES (DMA_BUF_SIZE_BYTES / sizeof(uint16_t)) // 1024 samples

/* 宣告 DMA 雙暫存器/循環緩衝區 */
static uint16_t adc_dma_buffer[DMA_BUF_SAMPLES] __aligned(4);

/**
 * @brief DMA2 Stream 0 中斷服務程式 (ISR)
 * 當 DMA 搬移滿一半 (1KB / 512 samples) 或全滿 (2KB / 1024 samples) 時觸發
 */
static void dma2_stream0_isr(const void *arg) {
    ARG_UNUSED(arg);

    // 檢查 Half-Transfer 中斷標誌 (前 1KB 資料填充完畢)
    if (DMA2->LISR & DMA_LISR_HTIF0) {
        DMA2->LIFCR = DMA_LIFCR_CHTIF0; // 清除 HTIF0 標誌

        // TODO: 處理前半段資料 adc_dma_buffer ~ adc_dma_buffer[22]
        printf("a\n");
    }

    // 檢查 Transfer-Complete 中斷標誌 (後 1KB 資料填充完畢)
    if (DMA2->LISR & DMA_LISR_TCIF0) {
        DMA2->LIFCR = DMA_LIFCR_CTCIF0; // 清除 TCIF0 標誌

        // TODO: 處理後半段資料 adc_dma_buffer[23] ~ adc_dma_buffer[24]
        printf("b\n");
    }
}

/**
 * @brief TIM3 中斷服務程式 (ISR)
 * 1kHz 頻率觸發
 */
static void tim3_isr(const void *arg) {
    ARG_UNUSED(arg);

    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR = ~TIM_SR_UIF; // 清除 Update Interrupt 標誌

        // TODO: 可在此放置 1kHz 週期性任務
    }
}


#include <zephyr/init.h>

/**
 * @brief 初始化 ADC1, TIM3, DMA2 及 GPIOA (PA0)
 */
static int adc_custom_init(void) {
    /* =========================================================================
     * 1. 致能週邊時脈 (RCC)
     * =========================================================================
     */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // 致能 GPIOA 時脈 [25]
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;  // 致能 DMA2 時脈 [25]
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;  // 致能 ADC1 時脈 [26]
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;  // 致能 TIM3 時脈 [27]

    /* =========================================================================
     * 2. 設定 PA0 為模擬輸入模式 (Analog Mode)
     * =========================================================================
     */
    GPIOA->MODER |=
        GPIO_MODER_MODE0_0 | GPIO_MODER_MODE0_1; // MODER0 = 11 (Analog) [28]
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPD0_0 |
                      GPIO_PUPDR_PUPD0_1); // PUPDR0 = 00 (No pull) [29]

    /* =========================================================================
     * 3. 設定 DMA2 Stream 0 Channel 0 (對應 ADC1)
     * =========================================================================
     */
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream0->CR & DMA_SxCR_EN)
        ; // 等待 Stream 完全關閉 [30]

    DMA2_Stream0->PAR =
        (uint32_t) & (ADC1->DR); // 週邊目標位址：ADC1 Data Register [31]
    DMA2_Stream0->M0AR =
        (uint32_t)adc_dma_buffer; // 記憶體目標位址：RAM Buffer [32]
    DMA2_Stream0->NDTR = DMA_BUF_SAMPLES; // 傳輸總筆數：1024 筆 Half-Word [33]

    DMA2_Stream0->CR =
        (0 << DMA_SxCR_CHSEL_Pos) | // Channel 0 [14, 34]
        DMA_SxCR_PL_1 |             // 優先級：High [35, 36]
        DMA_SxCR_MSIZE_0 | // 記憶體資料寬度：16-bit (Half-word) [18, 36]
        DMA_SxCR_PSIZE_0 | // 週邊資料寬度：16-bit (Half-word) [18, 36]
        DMA_SxCR_MINC |    // 記憶體地址自動遞增 [37, 38]
        DMA_SxCR_CIRC |    // 啟用循環模式 (Circular Mode) [39, 40]
        DMA_SxCR_HTIE |    // 滿一半 (Half Transfer) 中斷致能 [20, 21]
        DMA_SxCR_TCIE;     // 全滿 (Transfer Complete) 中斷致能 [20, 21]

    DMA2_Stream0->CR |= DMA_SxCR_EN; // 啟動 DMA Stream [20, 41]

    /* =========================================================================
     * 4. 設定 ADC1 (12-bit, 通道 0, 外部 TIM3 TRGO 觸發)
     * =========================================================================
     */
    ADC1_COMMON->CCR &= ~ADC_CCR_ADCPRE;
    ADC1_COMMON->CCR |=
        ADC_CCR_ADCPRE_0; // ADCCLK = PCLK2 / 4 = 96MHz / 4 = 24MHz [42]

    ADC1->CR1 = 0; // 12-bit 解析度，單次/掃描模式設為預設 [43]

    ADC1->CR2 =
        ADC_CR2_DMA | // 致能 ADC DMA [44]
        ADC_CR2_DDS | // 允許 DMA 在循環模式下持續發送請求 [44, 45]
        (0x8 << ADC_CR2_EXTSEL_Pos) | // EXTSEL = 1000b (TIM3 TRGO) [11, 12]
        (0x1 << ADC_CR2_EXTEN_Pos); // 觸發極性：上升沿 (Rising edge) [13]

    ADC1->SQR1 = 0; // 常規序列長度 L = 1 次轉換 [23]
    ADC1->SQR3 = 0; // 第 1 次轉換目標為 Channel 0 (PA0) [46]
    ADC1->SMPR2 |=
        (0x3 << ADC_SMPR2_SMP0_Pos); // Channel 0 採樣時間設為 56 週期 [47]

    ADC1->CR2 |= ADC_CR2_ADON; // 開啟 ADC 電源 [48, 49]

    /* =========================================================================
     * 5. 設定 TIM3 (產生 1kHz TRGO 訊號與 Update 中斷)
     * =========================================================================
     */
    TIM3->PSC = 95;  // Prescaler: 96MHz / (95 + 1) = 1MHz [5]
    TIM3->ARR = 999; // Period: 1MHz / (999 + 1) = 1kHz [6]

    TIM3->CR2 &= ~TIM_CR2_MMS;
    TIM3->CR2 |=
        (0x2 << TIM_CR2_MMS_Pos); // MMS = 010b (Update Event 作為 TRGO) [7, 8]
    TIM3->DIER |= TIM_DIER_UIE;   // 啟用 Update 中斷 [50, 51]

    /* =========================================================================
     * 6. 在 Zephyr 系統中註冊與開啟 NVIC 中斷
     * =========================================================================
     */
    // 註冊 DMA2 Stream 0 中斷 (IRQ 56) [52]
    IRQ_CONNECT(DMA2_Stream0_IRQn, 1, dma2_stream0_isr, NULL, 0);
    irq_enable(DMA2_Stream0_IRQn);

    // 註冊 TIM3 中斷 (IRQ 29) [53]
    IRQ_CONNECT(TIM3_IRQn, 2, tim3_isr, NULL, 0);
    irq_enable(TIM3_IRQn);

    /* 啟動 TIM3 開始計數並輸出 TRGO 觸發 ADC */
    TIM3->CR1 |= TIM_CR1_CEN; // [ 54, 55 ]

    return 0;
}

SYS_INIT(adc_custom_init, APPLICATION, 0);
