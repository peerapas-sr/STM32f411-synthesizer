/*******************************************************************************
 * File Name   : bsp_oled.c
 * Description : 1.30" SH1106 I2C OLED Driver - PB8 (SCL) / PB9 (SDA), AF4 Open-Drain
 *               Framebuffer pages streamed by DMA1 Stream 6 Channel 1 (I2C1_TX)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_oled.h"
#define STM32F411xE
#include "stm32f4xx.h"
#include "bsp_timer.h"

/* Named Constants (Rule 5 & Rule 10) */
#define I2C_OLED_ADDR_WRITE         (0x78U)    /* 0x3C << 1 */
#define I2C_TIMEOUT_CYCLES          (10000U)
#define I2C_CTRL_BYTE_CMD           (0x00U)
#define I2C_CTRL_BYTE_DATA          (0x40U)
#define I2C_ERROR_FLAGS             (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR)
#define I2C_SCL_BIT                 (1UL << 8U)
#define I2C_SDA_BIT                 (1UL << 9U)
#define I2C_PINS_2BIT_MASK          ((3UL << 16U) | (3UL << 18U))
#define I2C_PINS_OUTPUT_MODE        ((1UL << 16U) | (1UL << 18U))
#define I2C_PINS_AF_MODE            ((2UL << 16U) | (2UL << 18U))
#define I2C_PINS_PULL_UP            ((1UL << 16U) | (1UL << 18U))
#define I2C_PINS_VERY_HIGH          ((3UL << 16U) | (3UL << 18U))    /* OSPEEDR = 11: very high speed */
#define BSRR_RESET_SHIFT            (16U)
#define DMA_CHANNEL_1               (1UL)      /* DMA1 Stream 6 channel 1 = I2C1_TX */
#define I2C_PINS_AF4                ((4UL << 0U) | (4UL << 4U))
#define I2C_PINS_AF_MASK            ((15UL << 0U) | (15UL << 4U))
#define I2C_CR2_FREQ_16MHZ          (16U)
#define I2C_CCR_FAST_400KHZ         (14U)
#define I2C_TRISE_FAST              (5U)
#define I2C_RECOVERY_PULSES         (9U)
#define I2C_BIT_DELAY_US            (10U)
#define I2C_IDLE_DELAY_US           (5U)
#define DMA1_S6_ALL_FLAGS           (DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 | \
                                     DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6)
#define I2C1_DMA_NVIC_PRIORITY      (2U)

#define OLED_BUFFER_SIZE            (OLED_WIDTH_PX * OLED_NUM_PAGES)
#define OLED_PAGE_HEIGHT_PX         (8U)       /* One framebuffer byte = 8 vertical pixels */
#define OLED_ALL_PIXELS_ON          (0xFFU)
#define OLED_DMA_PAYLOAD_LEN        (OLED_WIDTH_PX + 1U)    /* Control byte + 128 data bytes */
#define OLED_SERVICE_SLICE_MS       (5U)
#define OLED_DMA_TIMEOUT_MS         (15U)
#define OLED_MAX_ERRORS             (3U)
#define OLED_KEEP_ALIVE_MS          (2000U)
#define OLED_POWER_UP_MS            (50U)
#define SH1106_PAGE_CMD_BASE        (0xB0U)
#define SH1106_COL_LOW_OFFSET       (0x02U)    /* 1.3" SH1106 RAM starts at column 2 */
#define SH1106_COL_HIGH_BASE        (0x10U)

#define FONT_WIDTH_PX               (5U)
#define FONT_CELL_PX                (6U)       /* 5 px glyph + 1 px gap */
#define FONT_FIRST_ASCII            (32U)
#define FONT_LAST_ASCII             (95U)
#define ASCII_LOWER_A               (97U)
#define ASCII_LOWER_Z               (122U)
#define ASCII_CASE_OFFSET           (32U)

#define PIANO_KEY_WIDTH_PX          (16U)
#define PIANO_TOP_Y                 (24U)
#define PIANO_BOT_Y                 (63U)
#define PIANO_BLACK_BOT_Y           (44U)
#define PIANO_BLACK_WIDTH_PX        (7U)
#define PIANO_NUM_BLACK_KEYS        (5U)
#define PIANO_LABEL_PAGE            (7U)
#define VOL_BAR_X0                  (96U)
#define VOL_BAR_X1                  (126U)
#define VOL_BAR_MAX_LEN             (28U)
#define GAUGE_SEP_Y                 (16U)
#define GAUGE_X0                    (40U)
#define GAUGE_X1                    (88U)
#define GAUGE_Y0                    (18U)
#define GAUGE_Y1                    (22U)
#define GAUGE_CENTER_X              (64)
#define GAUGE_HALF_TRAVEL           (20)
#define GAUGE_LABEL_LEFT_X          (2U)
#define GAUGE_LABEL_RIGHT_X         (96U)
#define JOY_NORM_FULL               (1000)
#define PERCENT_FULL                (100U)
#define PAGE_STATUS                 (0U)       /* Mode, bank, volume */
#define PAGE_NOTE                   (1U)       /* Note name, frequency, bend */
#define PAGE_GAUGE                  (2U)       /* Pitch gauge labels */
#define HDR_BANK_X                  (44U)
#define HDR_VOL_LABEL_X             (72U)
#define HDR_FREQ_X                  (50U)
#define HDR_BEND_X                  (88U)
#define VOL_BAR_Y0                  (1U)
#define VOL_BAR_Y1                  (6U)
#define PIANO_LABEL_X_OFFSET        (3U)
#define PIANO_FILL_PAGE_TOP         (3U)       /* Active key fill covers rows 25..62 = pages 3..7 */
#define PIANO_FILL_PAGE_BOT         (7U)
#define PIANO_FILL_TOP_BITS         (0xFEU)    /* Page 3: rows 25..31 (row 24 is the border) */
#define PIANO_FILL_BOT_BITS         (0x7FU)    /* Page 7: rows 56..62 (row 63 is the border) */

/* 5x7 ASCII Font (ASCII 32..95), one column byte per entry, 4 glyphs per row */
static const uint8_t OLED_FONT5X7[FONT_LAST_ASCII - FONT_FIRST_ASCII + 1U][FONT_WIDTH_PX] = {
    {0x00U, 0x00U, 0x00U, 0x00U, 0x00U}, {0x00U, 0x00U, 0x5FU, 0x00U, 0x00U}, {0x00U, 0x07U, 0x00U, 0x07U, 0x00U}, {0x14U, 0x7FU, 0x14U, 0x7FU, 0x14U}, /* sp ! " # */
    {0x24U, 0x2AU, 0x7FU, 0x2AU, 0x12U}, {0x23U, 0x13U, 0x08U, 0x64U, 0x62U}, {0x36U, 0x49U, 0x55U, 0x22U, 0x50U}, {0x00U, 0x05U, 0x03U, 0x00U, 0x00U}, /* $ % & ' */
    {0x00U, 0x1CU, 0x22U, 0x41U, 0x00U}, {0x00U, 0x41U, 0x22U, 0x1CU, 0x00U}, {0x14U, 0x08U, 0x3EU, 0x08U, 0x14U}, {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U}, /* ( ) * + */
    {0x00U, 0x50U, 0x30U, 0x00U, 0x00U}, {0x08U, 0x08U, 0x08U, 0x08U, 0x08U}, {0x00U, 0x60U, 0x60U, 0x00U, 0x00U}, {0x20U, 0x10U, 0x08U, 0x04U, 0x02U}, /* , - . / */
    {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U}, {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U}, /* 0 1 2 3 */
    {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, {0x27U, 0x45U, 0x45U, 0x45U, 0x39U}, {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, {0x01U, 0x71U, 0x09U, 0x05U, 0x03U}, /* 4 5 6 7 */
    {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}, {0x00U, 0x36U, 0x36U, 0x00U, 0x00U}, {0x00U, 0x56U, 0x36U, 0x00U, 0x00U}, /* 8 9 : ; */
    {0x08U, 0x14U, 0x22U, 0x41U, 0x00U}, {0x14U, 0x14U, 0x14U, 0x14U, 0x14U}, {0x00U, 0x41U, 0x22U, 0x14U, 0x08U}, {0x02U, 0x01U, 0x51U, 0x09U, 0x06U}, /* < = > ? */
    {0x32U, 0x49U, 0x79U, 0x41U, 0x3EU}, {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}, {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U}, {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}, /* @ A B C */
    {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU}, {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}, {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U}, {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}, /* D E F G */
    {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU}, {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}, {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U}, {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}, /* H I J K */
    {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}, {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU}, {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}, /* L M N O */
    {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U}, {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}, {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U}, {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}, /* P Q R S */
    {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U}, {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}, {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU}, {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}, /* T U V W */
    {0x63U, 0x14U, 0x08U, 0x14U, 0x63U}, {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}, {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}, {0x00U, 0x7FU, 0x41U, 0x41U, 0x00U}, /* X Y Z [ */
    {0x02U, 0x04U, 0x08U, 0x10U, 0x20U}, {0x00U, 0x41U, 0x41U, 0x7FU, 0x00U}, {0x04U, 0x02U, 0x01U, 0x02U, 0x04U}, {0x40U, 0x40U, 0x40U, 0x40U, 0x40U}  /* \ ] ^ _ */
};

static uint8_t           g_u1t_frame[OLED_BUFFER_SIZE];          /* Framebuffer: 8 pages x 128 columns */
static uint8_t           g_u1t_dma_buf[OLED_DMA_PAYLOAD_LEN];    /* Page snapshot streamed by DMA */
static volatile bool     g_b_dma_busy = false;
static uint8_t           g_u1t_page = 0U;
static uint8_t           g_u1t_errors = 0U;
static uint32_t          g_u4t_slice_ms = 0U;
static uint32_t          g_u4t_keep_alive_ms = 0U;

/* Wait (bounded) until at least one bit of the mask is 1; returns false on timeout */
static bool oled_wait_set(volatile uint32_t *p_reg, uint32_t u4t_mask)
{
    uint32_t u4t_timeout = I2C_TIMEOUT_CYCLES;

    while (((*p_reg & u4t_mask) == 0U) && (u4t_timeout > 0U))
    {
        u4t_timeout--;
    }
    return (u4t_timeout > 0U);
}

/* Wait (bounded) until all bits of the mask are 0; returns false on timeout */
static bool oled_wait_clear(volatile uint32_t *p_reg, uint32_t u4t_mask)
{
    uint32_t u4t_timeout = I2C_TIMEOUT_CYCLES;

    while (((*p_reg & u4t_mask) != 0U) && (u4t_timeout > 0U))
    {
        u4t_timeout--;
    }
    return (u4t_timeout > 0U);
}

/* Bus recovery: abort DMA, clock out a stuck slave (9 SCL pulses + STOP), re-init I2C1 @ 400 kHz */
static void oled_i2c_bus_recovery(void)
{
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    DMA1->HIFCR = DMA1_S6_ALL_FLAGS;
    I2C1->CR1 |= I2C_CR1_SWRST;

    GPIOB->MODER = (GPIOB->MODER & ~I2C_PINS_2BIT_MASK) | I2C_PINS_OUTPUT_MODE;
    GPIOB->BSRR = (I2C_SCL_BIT | I2C_SDA_BIT);
    bsp_delay_us(I2C_BIT_DELAY_US);
    for (uint8_t u1t_i = 0U; (u1t_i < I2C_RECOVERY_PULSES) && ((GPIOB->IDR & I2C_SDA_BIT) == 0U); u1t_i++)
    {
        GPIOB->BSRR = (I2C_SCL_BIT << BSRR_RESET_SHIFT);
        bsp_delay_us(I2C_BIT_DELAY_US);
        GPIOB->BSRR = I2C_SCL_BIT;
        bsp_delay_us(I2C_BIT_DELAY_US);
    }
    GPIOB->BSRR = (I2C_SDA_BIT << BSRR_RESET_SHIFT);    /* Manual STOP: SDA low -> SCL high -> SDA high */
    bsp_delay_us(I2C_BIT_DELAY_US);
    GPIOB->BSRR = I2C_SCL_BIT;
    bsp_delay_us(I2C_BIT_DELAY_US);
    GPIOB->BSRR = I2C_SDA_BIT;
    bsp_delay_us(I2C_BIT_DELAY_US);
    GPIOB->MODER = (GPIOB->MODER & ~I2C_PINS_2BIT_MASK) | I2C_PINS_AF_MODE;

    I2C1->CR1 &= ~I2C_CR1_SWRST;
    I2C1->CR2 = I2C_CR2_FREQ_16MHZ;
    I2C1->CCR = (I2C_CCR_FS | I2C_CCR_FAST_400KHZ);
    I2C1->TRISE = I2C_TRISE_FAST;
    I2C1->CR1 |= I2C_CR1_PE;
    g_b_dma_busy = false;
}

/* START + slave address; recovers the bus if it is stuck busy */
static bool oled_i2c_start(void)
{
    bool b_ok = oled_wait_clear(&I2C1->SR2, I2C_SR2_BUSY);

    if (b_ok == true)
    {
        I2C1->SR1 &= ~I2C_ERROR_FLAGS;
        I2C1->CR1 |= I2C_CR1_START;
        b_ok = oled_wait_set(&I2C1->SR1, I2C_SR1_SB);
    }
    else
    {
        /* No action required */
    }

    if (b_ok == false)
    {
        oled_i2c_bus_recovery();
    }
    else
    {
        I2C1->DR = I2C_OLED_ADDR_WRITE;
        (void)oled_wait_set(&I2C1->SR1, (I2C_SR1_ADDR | I2C_SR1_AF));
        if ((I2C1->SR1 & I2C_SR1_ADDR) != 0U)
        {
            (void)I2C1->SR2;    /* SR1 then SR2 read clears ADDR */
        }
        else
        {
            I2C1->SR1 &= ~I2C_SR1_AF;    /* No ACK from display */
            I2C1->CR1 |= I2C_CR1_STOP;
            b_ok = false;
        }
    }
    return b_ok;
}

/* Wait for last byte (BTF) or an error, then STOP and clear error flags */
static void oled_i2c_stop(void)
{
    (void)oled_wait_set(&I2C1->SR1, (I2C_SR1_BTF | I2C_SR1_AF | I2C_SR1_BERR));
    I2C1->CR1 |= I2C_CR1_STOP;
    (void)oled_wait_clear(&I2C1->CR1, I2C_CR1_STOP);
    I2C1->SR1 &= ~I2C_ERROR_FLAGS;
}

/* Send control byte + payload in one blocking I2C transaction (commands, or a data page at init) */
static bool oled_send(uint8_t u1t_ctrl, const uint8_t *p_bytes, uint8_t u1t_len)
{
    bool b_ok = oled_i2c_start();

    if (b_ok == true)
    {
        I2C1->DR = u1t_ctrl;
        for (uint8_t u1t_i = 0U; u1t_i < u1t_len; u1t_i++)
        {
            (void)oled_wait_set(&I2C1->SR1, I2C_SR1_TXE);
            I2C1->DR = p_bytes[u1t_i];
        }
        oled_i2c_stop();
        bsp_delay_us(I2C_IDLE_DELAY_US);
    }
    else
    {
        /* No action required */
    }
    return b_ok;
}

static bool oled_set_page(uint8_t u1t_page)
{
    const uint8_t PAGE_CMDS[] = {(uint8_t)(SH1106_PAGE_CMD_BASE | u1t_page), SH1106_COL_LOW_OFFSET, SH1106_COL_HIGH_BASE};
    return oled_send(I2C_CTRL_BYTE_CMD, PAGE_CMDS, (uint8_t)sizeof(PAGE_CMDS));
}

/* Set page address, then hand the 129-byte page payload to DMA (non-blocking) */
static bool oled_write_page_dma(uint8_t u1t_page)
{
    bool b_ok = oled_set_page(u1t_page);

    if (b_ok == true)
    {
        g_u1t_dma_buf[0] = I2C_CTRL_BYTE_DATA;
        for (uint16_t u2t_col = 0U; u2t_col < OLED_WIDTH_PX; u2t_col++)
        {
            g_u1t_dma_buf[u2t_col + 1U] = g_u1t_frame[((uint16_t)u1t_page * OLED_WIDTH_PX) + u2t_col];
        }
        DMA1_Stream6->CR &= ~DMA_SxCR_EN;
        (void)oled_wait_clear(&DMA1_Stream6->CR, DMA_SxCR_EN);
        DMA1->HIFCR = DMA1_S6_ALL_FLAGS;
        DMA1_Stream6->M0AR = (uint32_t)g_u1t_dma_buf;
        DMA1_Stream6->NDTR = OLED_DMA_PAYLOAD_LEN;
        b_ok = oled_i2c_start();
    }
    else
    {
        /* No action required */
    }

    if (b_ok == true)
    {
        g_b_dma_busy = true;
        I2C1->CR2 |= I2C_CR2_DMAEN;
        DMA1_Stream6->CR |= DMA_SxCR_EN;
    }
    else
    {
        /* No action required */
    }
    return b_ok;
}

/* DMA1 Stream 6 ISR: page transfer complete -> STOP and release the bus */
void DMA1_Stream6_IRQHandler(void)
{
    DMA1->HIFCR = DMA1_S6_ALL_FLAGS;
    I2C1->CR2 &= ~I2C_CR2_DMAEN;
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    oled_i2c_stop();
    g_b_dma_busy = false;
}

/* Keep-alive: re-enable charge pump / DC-DC and display ON */
static void oled_wake_display(void)
{
    static const uint8_t WAKE_CMDS[] = {0x8DU, 0x14U, 0xADU, 0x8BU, 0xAFU};
    (void)oled_send(I2C_CTRL_BYTE_CMD, WAKE_CMDS, (uint8_t)sizeof(WAKE_CMDS));
}

/* Called every superloop pass: one page per 5 ms slice, with stall watchdog and error recovery */
void bsp_oled_service(uint32_t u4t_now)
{
    if (g_b_dma_busy == true)
    {
        if ((u4t_now - g_u4t_slice_ms) > OLED_DMA_TIMEOUT_MS)
        {
            oled_i2c_bus_recovery();    /* DMA transfer stalled */
        }
        else
        {
            /* No action required */
        }
    }
    else if ((u4t_now - g_u4t_slice_ms) >= OLED_SERVICE_SLICE_MS)
    {
        g_u4t_slice_ms = u4t_now;
        if ((g_u1t_page == 0U) && ((u4t_now - g_u4t_keep_alive_ms) >= OLED_KEEP_ALIVE_MS))
        {
            g_u4t_keep_alive_ms = u4t_now;
            oled_wake_display();
        }
        else
        {
            /* No action required */
        }

        if (oled_write_page_dma(g_u1t_page) == true)
        {
            g_u1t_errors = 0U;
            g_u1t_page = (uint8_t)((g_u1t_page + 1U) % OLED_NUM_PAGES);
        }
        else
        {
            g_u1t_errors++;    /* Retry this page on the next slice; recover after 3 failures */
            if (g_u1t_errors >= OLED_MAX_ERRORS)
            {
                g_u1t_errors = 0U;
                oled_i2c_bus_recovery();
                oled_wake_display();
            }
            else
            {
                /* No action required */
            }
        }
    }
    else
    {
        /* Waiting for next slice */
    }
}

/* Graphics Primitives */
void bsp_oled_clear_buffer(void)
{
    for (uint16_t u2t_i = 0U; u2t_i < OLED_BUFFER_SIZE; u2t_i++)
    {
        g_u1t_frame[u2t_i] = 0U;
    }
}

/* Fill (or clear) the inclusive rectangle; lines are 1-pixel rectangles */
static void oled_fill_rect(uint8_t u1t_x0, uint8_t u1t_y0, uint8_t u1t_x1, uint8_t u1t_y1, bool b_color)
{
    for (uint8_t u1t_y = u1t_y0; u1t_y <= u1t_y1; u1t_y++)
    {
        uint8_t u1t_bit = (uint8_t)(1U << (u1t_y % OLED_PAGE_HEIGHT_PX));
        for (uint8_t u1t_x = u1t_x0; u1t_x <= u1t_x1; u1t_x++)
        {
            uint16_t u2t_idx = ((uint16_t)(u1t_y / OLED_PAGE_HEIGHT_PX) * OLED_WIDTH_PX) + u1t_x;
            if (b_color == true)
            {
                g_u1t_frame[u2t_idx] |= u1t_bit;
            }
            else
            {
                g_u1t_frame[u2t_idx] &= (uint8_t)(~u1t_bit);
            }
        }
    }
}

static void oled_draw_box(uint8_t u1t_x0, uint8_t u1t_y0, uint8_t u1t_x1, uint8_t u1t_y1)
{
    oled_fill_rect(u1t_x0, u1t_y0, u1t_x1, u1t_y0, true);
    oled_fill_rect(u1t_x0, u1t_y1, u1t_x1, u1t_y1, true);
    oled_fill_rect(u1t_x0, u1t_y0, u1t_x0, u1t_y1, true);
    oled_fill_rect(u1t_x1, u1t_y0, u1t_x1, u1t_y1, true);
}

/* Draw text on a page (8-px row); lowercase shown as uppercase, unknown chars blank */
static void oled_draw_string(uint8_t u1t_x, uint8_t u1t_page, const char *p_str, bool b_invert)
{
    uint8_t u1t_cx = u1t_x;
    const char *p_ch = p_str;

    while ((*p_ch != '\0') && (u1t_cx <= (OLED_WIDTH_PX - FONT_CELL_PX)))
    {
        uint8_t u1t_c = (uint8_t)*p_ch;
        if ((u1t_c >= ASCII_LOWER_A) && (u1t_c <= ASCII_LOWER_Z))
        {
            u1t_c -= ASCII_CASE_OFFSET;
        }
        else
        {
            /* No action required */
        }
        for (uint8_t u1t_col = 0U; u1t_col < FONT_CELL_PX; u1t_col++)
        {
            uint8_t u1t_bits = 0U;
            if ((u1t_col < FONT_WIDTH_PX) && (u1t_c >= FONT_FIRST_ASCII) && (u1t_c <= FONT_LAST_ASCII))
            {
                u1t_bits = OLED_FONT5X7[u1t_c - FONT_FIRST_ASCII][u1t_col];
            }
            else
            {
                /* No action required */
            }
            if (b_invert == true)
            {
                u1t_bits = (uint8_t)(~u1t_bits);    /* White text on black for the active key label */
            }
            else
            {
                /* No action required */
            }
            g_u1t_frame[((uint16_t)u1t_page * OLED_WIDTH_PX) + u1t_cx + u1t_col] = u1t_bits;
        }
        u1t_cx += FONT_CELL_PX;
        p_ch++;
    }
}

/* Virtual Piano UI Renderers */
void bsp_oled_render_header(const char *p_mode, bool b_high_bank, uint8_t u1t_vol_pct,
                            const char *p_note_name, const char *p_note_freq, int32_t s4t_cents)
{
    uint8_t u1t_fill = (uint8_t)(((uint32_t)u1t_vol_pct * VOL_BAR_MAX_LEN) / PERCENT_FULL);
    const char *p_bank = "[LO]";
    const char *p_bend = "P: 0";

    if (b_high_bank == true)
    {
        p_bank = "[HI]";
    }
    else
    {
        /* No action required */
    }
    if (s4t_cents > 0)
    {
        p_bend = "P:+";
    }
    else if (s4t_cents < 0)
    {
        p_bend = "P:-";
    }
    else
    {
        /* No bend */
    }

    /* Page 0: mode, bank, volume bar | Page 1: note name, frequency, bend direction */
    oled_draw_string(0U, PAGE_STATUS, p_mode, false);
    oled_draw_string(HDR_BANK_X, PAGE_STATUS, p_bank, false);
    oled_draw_string(HDR_VOL_LABEL_X, PAGE_STATUS, "VOL:", false);
    oled_draw_box(VOL_BAR_X0, VOL_BAR_Y0, VOL_BAR_X1, VOL_BAR_Y1);
    if (u1t_fill > 0U)
    {
        oled_fill_rect(VOL_BAR_X0 + 1U, VOL_BAR_Y0 + 1U, (uint8_t)(VOL_BAR_X0 + 1U + u1t_fill), VOL_BAR_Y1 - 1U, true);
    }
    else
    {
        /* No action required */
    }
    oled_draw_string(0U, PAGE_NOTE, p_note_name, false);
    oled_draw_string(HDR_FREQ_X, PAGE_NOTE, p_note_freq, false);
    oled_draw_string(HDR_BEND_X, PAGE_NOTE, p_bend, false);
}

void bsp_oled_render_pitch_gauge(int32_t s4t_norm_x)
{
    int32_t s4t_dot_x = GAUGE_CENTER_X + ((s4t_norm_x * GAUGE_HALF_TRAVEL) / JOY_NORM_FULL);    /* 44..84 */

    oled_fill_rect(0U, GAUGE_SEP_Y, OLED_WIDTH_PX - 1U, GAUGE_SEP_Y, true);
    oled_draw_string(GAUGE_LABEL_LEFT_X, PAGE_GAUGE, "PITCH", false);
    oled_draw_string(GAUGE_LABEL_RIGHT_X, PAGE_GAUGE, "ROLL", false);
    oled_draw_box(GAUGE_X0, GAUGE_Y0, GAUGE_X1, GAUGE_Y1);
    oled_fill_rect((uint8_t)GAUGE_CENTER_X, GAUGE_Y0 + 1U, (uint8_t)GAUGE_CENTER_X, GAUGE_Y1 - 1U, true);
    oled_fill_rect((uint8_t)(s4t_dot_x - 1), GAUGE_Y0 + 1U, (uint8_t)(s4t_dot_x + 1), GAUGE_Y1 - 1U, true);
}

void bsp_oled_render_piano_keyboard(int8_t s1t_active_key, const char * const pp_labels[OLED_PIANO_NUM_KEYS])
{
    static const uint8_t BLACK_KEY_X[PIANO_NUM_BLACK_KEYS] = {12U, 28U, 60U, 76U, 92U};

    oled_fill_rect(0U, PIANO_TOP_Y, OLED_WIDTH_PX - 1U, PIANO_TOP_Y, true);
    oled_fill_rect(0U, PIANO_BOT_Y, OLED_WIDTH_PX - 1U, PIANO_BOT_Y, true);
    for (uint8_t u1t_k = 0U; u1t_k < OLED_PIANO_NUM_KEYS; u1t_k++)
    {
        uint8_t u1t_x0 = u1t_k * PIANO_KEY_WIDTH_PX;
        uint8_t u1t_x1 = (u1t_x0 + PIANO_KEY_WIDTH_PX) - 1U;
        bool b_active = (s1t_active_key == (int8_t)u1t_k);

        oled_fill_rect(u1t_x0, PIANO_TOP_Y, u1t_x0, PIANO_BOT_Y, true);
        oled_fill_rect(u1t_x1, PIANO_TOP_Y, u1t_x1, PIANO_BOT_Y, true);
        if (b_active == true)
        {
            /* Page-byte stride fill: write whole bytes (8 rows each) instead of single pixels */
            for (uint8_t u1t_col = u1t_x0 + 1U; u1t_col < u1t_x1; u1t_col++)
            {
                g_u1t_frame[(PIANO_FILL_PAGE_TOP * OLED_WIDTH_PX) + u1t_col] |= PIANO_FILL_TOP_BITS;
                for (uint8_t u1t_page = PIANO_FILL_PAGE_TOP + 1U; u1t_page < PIANO_FILL_PAGE_BOT; u1t_page++)
                {
                    g_u1t_frame[((uint16_t)u1t_page * OLED_WIDTH_PX) + u1t_col] = OLED_ALL_PIXELS_ON;
                }
                g_u1t_frame[(PIANO_FILL_PAGE_BOT * OLED_WIDTH_PX) + u1t_col] |= PIANO_FILL_BOT_BITS;
            }
        }
        else
        {
            /* No action required */
        }
        oled_draw_string(u1t_x0 + PIANO_LABEL_X_OFFSET, PIANO_LABEL_PAGE, pp_labels[u1t_k], b_active);
    }

    /* Black keys (C#, D#, F#, G#, A#): filled block with cleared outline columns */
    for (uint8_t u1t_b = 0U; u1t_b < PIANO_NUM_BLACK_KEYS; u1t_b++)
    {
        uint8_t u1t_bx = BLACK_KEY_X[u1t_b];
        oled_fill_rect(u1t_bx, PIANO_TOP_Y + 1U, u1t_bx + PIANO_BLACK_WIDTH_PX, PIANO_BLACK_BOT_Y, true);
        oled_fill_rect(u1t_bx, PIANO_TOP_Y + 1U, u1t_bx, PIANO_BLACK_BOT_Y, false);
        oled_fill_rect(u1t_bx + PIANO_BLACK_WIDTH_PX, PIANO_TOP_Y + 1U, u1t_bx + PIANO_BLACK_WIDTH_PX, PIANO_BLACK_BOT_Y, false);
    }
}

/* Initialization: GPIO AF4 open-drain, I2C1 400 kHz, SH1106 command table, DMA1 Stream 6 */
void bsp_oled_init(void)
{
    /* Display OFF, clock div, mux 64, offset 0, start line 0, SSD1306 charge pump, SH1106 DC-DC, segment/COM remap,
     * COM pins, contrast, pre-charge, VCOMH, page addressing, RAM display, normal (non-inverted), display ON */
    static const uint8_t INIT_CMDS[] = {0xAEU, 0xD5U, 0x80U, 0xA8U, 0x3FU, 0xD3U, 0x00U, 0x40U, 0x8DU, 0x14U,
                                        0xADU, 0x8BU, 0xA1U, 0xC8U, 0xDAU, 0x12U, 0x81U, 0xCFU, 0xD9U, 0xF1U,
                                        0xDBU, 0x40U, 0x20U, 0x02U, 0xA4U, 0xA6U, 0xAFU};

    RCC->AHB1ENR |= (RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA1EN);
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    GPIOB->OTYPER |= (I2C_SCL_BIT | I2C_SDA_BIT);
    GPIOB->OSPEEDR |= I2C_PINS_VERY_HIGH;
    GPIOB->PUPDR = (GPIOB->PUPDR & ~I2C_PINS_2BIT_MASK) | I2C_PINS_PULL_UP;
    GPIOB->AFR[1] = (GPIOB->AFR[1] & ~I2C_PINS_AF_MASK) | I2C_PINS_AF4;
    oled_i2c_bus_recovery();    /* Also switches PB8/PB9 to AF mode and configures I2C1 */

    bsp_delay_ms(OLED_POWER_UP_MS);
    (void)oled_send(I2C_CTRL_BYTE_CMD, INIT_CMDS, (uint8_t)sizeof(INIT_CMDS));
    bsp_delay_ms(OLED_POWER_UP_MS);
    bsp_oled_clear_buffer();
    for (uint8_t u1t_p = 0U; u1t_p < OLED_NUM_PAGES; u1t_p++)
    {
        if (oled_set_page(u1t_p) == true)
        {
            (void)oled_send(I2C_CTRL_BYTE_DATA, &g_u1t_frame[(uint16_t)u1t_p * OLED_WIDTH_PX], (uint8_t)OLED_WIDTH_PX);
        }
        else
        {
            /* No action required */
        }
    }

    /* DMA1 Stream 6 Channel 1 (I2C1_TX): memory-to-peripheral, byte size, memory increment, TC interrupt */
    DMA1_Stream6->CR = 0U;
    (void)oled_wait_clear(&DMA1_Stream6->CR, DMA_SxCR_EN);
    DMA1->HIFCR = DMA1_S6_ALL_FLAGS;
    DMA1_Stream6->PAR = (uint32_t)(&(I2C1->DR));
    DMA1_Stream6->CR = ((DMA_CHANNEL_1 << DMA_SxCR_CHSEL_Pos) | DMA_SxCR_PL_1 | DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE);
    DMA1_Stream6->FCR = 0U;
    NVIC_SetPriority(DMA1_Stream6_IRQn, I2C1_DMA_NVIC_PRIORITY);
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}
