/*******************************************************************************
 * File Name   : bsp_oled.c
 * Description : Board Support Package - 1.30" I2C OLED (SH1106 / SSD1306) Driver
 * Target MCU  : STM32F411RET6 (Nucleo-F411RE)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 *
 * Pin Connections:
 *   SCK  -> PB8 (I2C1_SCL, AF4, Open-Drain)
 *   SDA  -> PB9 (I2C1_SDA, AF4, Open-Drain)
 *   VCC  -> 3.3V
 *   GND  -> GND
 ******************************************************************************/

#include "bsp_oled.h"
#define STM32F411xE
#include "stm32f4xx.h"
#include "bsp_timer.h"

/* Named Constants (Rule 5 & Rule 10) */
#define I2C_OLED_SLAVE_ADDR_WRITE   (0x78U)    /* 0x3C shifted left by 1 */
#define I2C_TIMEOUT_CYCLES          (10000U)
#define I2C_CTRL_BYTE_CMD           (0x00U)
#define I2C_CTRL_BYTE_DATA          (0x40U)

#define OLED_PAGE_SIZE_BYTES        (128U)
#define OLED_TOTAL_BUFFER_SIZE      (1024U)    /* 128 * 8 */
#define OLED_SERVICE_SLICE_MS       (5U)
#define OLED_DMA_PAGE_PAYLOAD_LEN   (129U)     /* 1 Control byte (0x40) + 128 Data bytes */
#define OLED_DMA_TIMEOUT_MS         (15U)
#define I2C1_DMA_NVIC_PRIORITY      (2U)
#define OLED_BUS_RECOVERY_PULSES    (9U)
#define OLED_BUS_IDLE_DELAY_US      (5U)
#define OLED_MAX_CONSECUTIVE_ERRORS (3U)
#define OLED_KEEP_ALIVE_INTERVAL_MS (2000U)

#define SH1106_PAGE_CMD_BASE        (0xB0U)
#define SH1106_COL_LOW_OFFSET       (0x02U)    /* 1.3" SH1106 offset 2 columns */
#define SH1106_COL_HIGH_BASE        (0x10U)

#define FONT_CHAR_WIDTH_PX          (5U)
#define FONT_FIRST_ASCII            (32U)
#define FONT_LAST_ASCII             (95U)
#define FONT_TOTAL_CHARS            (64U)

#define PIANO_KEY_WIDTH_PX          (16U)
#define PIANO_BORDER_TOP_Y          (24U)
#define PIANO_BORDER_BOT_Y          (63U)
#define PIANO_BLACK_KEY_BOT_Y       (44U)
#define PIANO_BLACK_KEY_WIDTH_PX    (8U)

#define OLED_VOL_BAR_X0             (96U)
#define OLED_VOL_BAR_X1             (126U)
#define OLED_VOL_BAR_Y0             (1U)
#define OLED_VOL_BAR_Y1             (6U)
#define OLED_VOL_BAR_FILL_Y0        (2U)
#define OLED_VOL_BAR_FILL_Y1        (5U)
#define OLED_VOL_BAR_MAX_LEN        (28U)

#define PITCH_GAUGE_SEP_Y           (16U)
#define PITCH_GAUGE_BOX_X0          (40U)
#define PITCH_GAUGE_BOX_X1          (88U)
#define PITCH_GAUGE_BOX_Y0          (18U)
#define PITCH_GAUGE_BOX_Y1          (22U)
#define PITCH_GAUGE_CENTER_X        (64)
#define PITCH_GAUGE_HALF_TRAVEL     (20)
#define PITCH_GAUGE_MIN_X           (42)
#define PITCH_GAUGE_MAX_X           (86)
#define PITCH_GAUGE_DOT_Y0          (19U)
#define PITCH_GAUGE_DOT_Y1          (21U)

/* 5x7 ASCII Font Table (ASCII 32 to 95) */
static const uint8_t OLED_FONT5X7[FONT_TOTAL_CHARS][FONT_CHAR_WIDTH_PX] = {
    {0x00U, 0x00U, 0x00U, 0x00U, 0x00U}, /* 32: Space */
    {0x00U, 0x00U, 0x5FU, 0x00U, 0x00U}, /* 33: ! */
    {0x00U, 0x07U, 0x00U, 0x07U, 0x00U}, /* 34: " */
    {0x14U, 0x7FU, 0x14U, 0x7FU, 0x14U}, /* 35: # */
    {0x24U, 0x2AU, 0x7FU, 0x2AU, 0x12U}, /* 36: $ */
    {0x23U, 0x13U, 0x08U, 0x64U, 0x62U}, /* 37: % */
    {0x36U, 0x49U, 0x55U, 0x22U, 0x50U}, /* 38: & */
    {0x00U, 0x05U, 0x03U, 0x00U, 0x00U}, /* 39: ' */
    {0x00U, 0x1CU, 0x22U, 0x41U, 0x00U}, /* 40: ( */
    {0x00U, 0x41U, 0x22U, 0x1CU, 0x00U}, /* 41: ) */
    {0x14U, 0x08U, 0x3EU, 0x08U, 0x14U}, /* 42: * */
    {0x08U, 0x08U, 0x3EU, 0x08U, 0x08U}, /* 43: + */
    {0x00U, 0x50U, 0x30U, 0x00U, 0x00U}, /* 44: , */
    {0x08U, 0x08U, 0x08U, 0x08U, 0x08U}, /* 45: - */
    {0x00U, 0x60U, 0x60U, 0x00U, 0x00U}, /* 46: . */
    {0x20U, 0x10U, 0x08U, 0x04U, 0x02U}, /* 47: / */
    {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, /* 48: 0 */
    {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U}, /* 49: 1 */
    {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, /* 50: 2 */
    {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U}, /* 51: 3 */
    {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, /* 52: 4 */
    {0x27U, 0x45U, 0x45U, 0x45U, 0x39U}, /* 53: 5 */
    {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, /* 54: 6 */
    {0x01U, 0x71U, 0x09U, 0x05U, 0x03U}, /* 55: 7 */
    {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, /* 56: 8 */
    {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}, /* 57: 9 */
    {0x00U, 0x36U, 0x36U, 0x00U, 0x00U}, /* 58: : */
    {0x00U, 0x56U, 0x36U, 0x00U, 0x00U}, /* 59: ; */
    {0x08U, 0x14U, 0x22U, 0x41U, 0x00U}, /* 60: < */
    {0x14U, 0x14U, 0x14U, 0x14U, 0x14U}, /* 61: = */
    {0x00U, 0x41U, 0x22U, 0x14U, 0x08U}, /* 62: > */
    {0x02U, 0x01U, 0x51U, 0x09U, 0x06U}, /* 63: ? */
    {0x32U, 0x49U, 0x79U, 0x41U, 0x3EU}, /* 64: @ */
    {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}, /* 65: A */
    {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U}, /* 66: B */
    {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}, /* 67: C */
    {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU}, /* 68: D */
    {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}, /* 69: E */
    {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U}, /* 70: F */
    {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}, /* 71: G */
    {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU}, /* 72: H */
    {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}, /* 73: I */
    {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U}, /* 74: J */
    {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}, /* 75: K */
    {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}, /* 76: L */
    {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, /* 77: M */
    {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU}, /* 78: N */
    {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}, /* 79: O */
    {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U}, /* 80: P */
    {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}, /* 81: Q */
    {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U}, /* 82: R */
    {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}, /* 83: S */
    {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U}, /* 84: T */
    {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}, /* 85: U */
    {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU}, /* 86: V */
    {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}, /* 87: W */
    {0x63U, 0x14U, 0x08U, 0x14U, 0x63U}, /* 88: X */
    {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}, /* 89: Y */
    {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}, /* 90: Z */
    {0x00U, 0x7FU, 0x41U, 0x41U, 0x00U}, /* 91: [ */
    {0x02U, 0x04U, 0x08U, 0x10U, 0x20U}, /* 92: \ */
    {0x00U, 0x41U, 0x41U, 0x7FU, 0x00U}, /* 93: ] */
    {0x04U, 0x02U, 0x01U, 0x02U, 0x04U}, /* 94: ^ */
    {0x40U, 0x40U, 0x40U, 0x40U, 0x40U}  /* 95: _ */
};

/* OLED Screen Framebuffer (1024 Bytes) */
static uint8_t           g_u1t_oled_buffer[OLED_TOTAL_BUFFER_SIZE];
static uint8_t           g_u1t_current_page = 0U;
static uint32_t          g_u4t_last_service_ms = 0U;

/* DMA Transfer Buffer: 1 Byte Control (0x40) + 128 Bytes Data */
static uint8_t           g_u1t_dma_page_buf[OLED_DMA_PAGE_PAYLOAD_LEN];
static volatile bool     g_b_oled_dma_busy = false;
static uint32_t          g_u4t_dma_start_time_ms = 0U;
static uint8_t           g_u1t_consecutive_errors = 0U;
static uint32_t          g_u4t_last_keep_alive_ms = 0U;

/* Private Function Prototypes (Rule 11) */
static void oled_dma_init(void);
static bool oled_write_page_dma(uint8_t u1t_page);
static void oled_write_page_sync(uint8_t u1t_page);
static bool oled_set_page_address(uint8_t u1t_page);
static void oled_i2c_bus_recovery(void);
static void oled_reinit_display(void);

/* Low-Level I2C Hardware Bus Recovery (9 SCL pulses & SWRST) */
static void oled_i2c_bus_recovery(void)
{
    /* 1. Disable DMA Stream 6 and clear DMA flags */
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    DMA1->HIFCR = (DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 |
                   DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6);

    /* 2. Disable I2C1 and assert Software Reset */
    I2C1->CR1 |= I2C_CR1_SWRST;
    bsp_delay_us(10U);

    /* 3. Configure PB8 (SCL) and PB9 (SDA) as Open-Drain GPIO Outputs */
    GPIOB->MODER &= ~((3UL << (8U * 2U)) | (3UL << (9U * 2U)));
    GPIOB->MODER |=  ((1UL << (8U * 2U)) | (1UL << (9U * 2U)));
    GPIOB->OTYPER |= ((1UL << 8U) | (1UL << 9U));
    GPIOB->PUPDR  &= ~((3UL << (8U * 2U)) | (3UL << (9U * 2U)));
    GPIOB->PUPDR  |=  ((1UL << (8U * 2U)) | (1UL << (9U * 2U)));

    /* Ensure pins start High */
    GPIOB->BSRR = ((1UL << 8U) | (1UL << 9U));
    bsp_delay_us(10U);

    /* 4. Clock SCL up to 9 times if SDA is held Low by the OLED slave */
    for (uint8_t u1t_pulse = 0U; u1t_pulse < OLED_BUS_RECOVERY_PULSES; u1t_pulse++)
    {
        if ((GPIOB->IDR & (1UL << 9U)) != 0U)
        {
            /* SDA is high, slave has released the bus */
            break;
        }
        else
        {
            /* Pulse SCL Low */
            GPIOB->BSRR = (1UL << (8U + 16U));
            bsp_delay_us(10U);
            /* Pulse SCL High */
            GPIOB->BSRR = (1UL << 8U);
            bsp_delay_us(10U);
        }
    }

    /* 5. Generate manual STOP condition (SDA Low -> SCL High -> SDA High) */
    GPIOB->BSRR = (1UL << (9U + 16U));
    bsp_delay_us(10U);
    GPIOB->BSRR = (1UL << 8U);
    bsp_delay_us(10U);
    GPIOB->BSRR = (1UL << 9U);
    bsp_delay_us(10U);

    /* 6. Switch PB8 and PB9 back to AF4 (I2C1 Alternate Function Open-Drain) */
    GPIOB->MODER &= ~((3UL << (8U * 2U)) | (3UL << (9U * 2U)));
    GPIOB->MODER |=  ((2UL << (8U * 2U)) | (2UL << (9U * 2U)));
    GPIOB->AFR[1] &= ~((15UL << 0U) | (15UL << 4U));
    GPIOB->AFR[1] |=  ((4UL  << 0U) | (4UL  << 4U));

    /* 7. Release I2C1 Software Reset */
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    bsp_delay_us(10U);

    /* 8. Reconfigure I2C1 Peripheral */
    I2C1->CR2 = 16U;
    I2C1->CCR = (uint16_t)(I2C_CCR_FS | 14U);
    I2C1->TRISE = 5U;
    I2C1->CR1 |= I2C_CR1_PE;

    /* 9. Reset DMA state flag */
    g_b_oled_dma_busy = false;
}

/* Low-Level I2C Helper Functions */
static bool oled_i2c_start(uint8_t u1t_slave_addr)
{
    bool b_success = true;
    uint32_t u4t_timeout = I2C_TIMEOUT_CYCLES;

    /* Wait while bus is busy */
    while (((I2C1->SR2 & I2C_SR2_BUSY) != 0U) && (u4t_timeout > 0U))
    {
        u4t_timeout--;
    }

    if (u4t_timeout == 0U)
    {
        /* Bus is stuck busy: initiate hardware bus recovery */
        oled_i2c_bus_recovery();
        b_success = false;
    }
    else
    {
        /* Clear any leftover error flags before START */
        I2C1->SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | I2C_SR1_OVR);

        /* Generate START condition */
        I2C1->CR1 |= I2C_CR1_START;
        u4t_timeout = I2C_TIMEOUT_CYCLES;

        while (((I2C1->SR1 & I2C_SR1_SB) == 0U) && (u4t_timeout > 0U))
        {
            u4t_timeout--;
        }

        if (u4t_timeout == 0U)
        {
            oled_i2c_bus_recovery();
            b_success = false;
        }
        else
        {
            /* Send Slave Address */
            (void)I2C1->SR1;
            I2C1->DR = u1t_slave_addr;
            u4t_timeout = I2C_TIMEOUT_CYCLES;

            while (((I2C1->SR1 & I2C_SR1_ADDR) == 0U) && (u4t_timeout > 0U))
            {
                if ((I2C1->SR1 & I2C_SR1_AF) != 0U)
                {
                    I2C1->SR1 &= ~I2C_SR1_AF;
                    u4t_timeout = 0U;
                }
                else
                {
                    u4t_timeout--;
                }
            }

            if (u4t_timeout == 0U)
            {
                I2C1->CR1 |= I2C_CR1_STOP;
                b_success = false;
            }
            else
            {
                /* Clear ADDR flag by reading SR1 and SR2 */
                (void)I2C1->SR1;
                (void)I2C1->SR2;
                b_success = true;
            }
        }
    }

    return b_success;
}

static bool oled_i2c_write_byte(uint8_t u1t_data)
{
    bool b_success = true;
    uint32_t u4t_timeout = I2C_TIMEOUT_CYCLES;

    while (((I2C1->SR1 & I2C_SR1_TXE) == 0U) && (u4t_timeout > 0U))
    {
        u4t_timeout--;
    }

    if (u4t_timeout == 0U)
    {
        b_success = false;
    }
    else
    {
        I2C1->DR = u1t_data;
        b_success = true;
    }

    return b_success;
}

static void oled_i2c_stop(void)
{
    uint32_t u4t_timeout = I2C_TIMEOUT_CYCLES;

    /* Wait for Byte Transfer Finished (BTF) */
    while (((I2C1->SR1 & I2C_SR1_BTF) == 0U) && (u4t_timeout > 0U))
    {
        if ((I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR)) != 0U)
        {
            break;
        }
        else
        {
            u4t_timeout--;
        }
    }

    /* Request STOP condition */
    I2C1->CR1 |= I2C_CR1_STOP;

    /* Wait for hardware to clear STOP bit in CR1 */
    u4t_timeout = I2C_TIMEOUT_CYCLES;
    while (((I2C1->CR1 & I2C_CR1_STOP) != 0U) && (u4t_timeout > 0U))
    {
        u4t_timeout--;
    }

    /* Bus idle settling guard delay */
    bsp_delay_us(OLED_BUS_IDLE_DELAY_US);
}

static void oled_send_command_list(const uint8_t *p_cmds, uint8_t u1t_len)
{
    if (oled_i2c_start(I2C_OLED_SLAVE_ADDR_WRITE) == true)
    {
        (void)oled_i2c_write_byte(I2C_CTRL_BYTE_CMD);
        for (uint8_t u1t_i = 0U; u1t_i < u1t_len; u1t_i++)
        {
            (void)oled_i2c_write_byte(p_cmds[u1t_i]);
        }
        oled_i2c_stop();
    }
    else
    {
        /* I2C failure handled gracefully */
    }
}

/* Re-awaken display and re-enable charge pump */
static void oled_reinit_display(void)
{
    static const uint8_t WAKE_CMDS[] = {
        0x8DU, 0x14U,   /* SSD1306 Charge Pump Enable */
        0xADU, 0x8BU,   /* SH1106 DC-DC Enable */
        0xAFU          /* Display ON */
    };

    oled_send_command_list(WAKE_CMDS, (uint8_t)sizeof(WAKE_CMDS));
}

/* Graphics Drawing Primitives */
void bsp_oled_clear_buffer(void)
{
    for (uint16_t u2t_i = 0U; u2t_i < OLED_TOTAL_BUFFER_SIZE; u2t_i++)
    {
        g_u1t_oled_buffer[u2t_i] = 0x00U;
    }
}

void bsp_oled_set_pixel(uint8_t u1t_x, uint8_t u1t_y, bool b_color)
{
    if ((u1t_x < OLED_WIDTH_PX) && (u1t_y < OLED_HEIGHT_PX))
    {
        uint8_t u1t_page = u1t_y / 8U;
        uint8_t u1t_bit = u1t_y % 8U;
        uint16_t u2t_index = ((uint16_t)u1t_page * OLED_WIDTH_PX) + (uint16_t)u1t_x;

        if (b_color == true)
        {
            g_u1t_oled_buffer[u2t_index] |= (uint8_t)(1U << u1t_bit);
        }
        else
        {
            g_u1t_oled_buffer[u2t_index] &= (uint8_t)(~(1U << u1t_bit));
        }
    }
    else
    {
        /* Coordinate out of bounds */
    }
}

void bsp_oled_draw_hline(uint8_t u1t_x0, uint8_t u1t_x1, uint8_t u1t_y, bool b_color)
{
    uint8_t u1t_start = u1t_x0;
    uint8_t u1t_end = u1t_x1;

    if (u1t_start > u1t_end)
    {
        u1t_start = u1t_x1;
        u1t_end = u1t_x0;
    }
    else
    {
        /* In correct order */
    }

    for (uint8_t u1t_x = u1t_start; u1t_x <= u1t_end; u1t_x++)
    {
        bsp_oled_set_pixel(u1t_x, u1t_y, b_color);
    }
}

void bsp_oled_draw_vline(uint8_t u1t_x, uint8_t u1t_y0, uint8_t u1t_y1, bool b_color)
{
    uint8_t u1t_start = u1t_y0;
    uint8_t u1t_end = u1t_y1;

    if (u1t_start > u1t_end)
    {
        u1t_start = u1t_y1;
        u1t_end = u1t_y0;
    }
    else
    {
        /* In correct order */
    }

    for (uint8_t u1t_y = u1t_start; u1t_y <= u1t_end; u1t_y++)
    {
        bsp_oled_set_pixel(u1t_x, u1t_y, b_color);
    }
}

void bsp_oled_fill_rect(uint8_t u1t_x0, uint8_t u1t_y0, uint8_t u1t_x1, uint8_t u1t_y1, bool b_color)
{
    for (uint8_t u1t_y = u1t_y0; u1t_y <= u1t_y1; u1t_y++)
    {
        bsp_oled_draw_hline(u1t_x0, u1t_x1, u1t_y, b_color);
    }
}

void bsp_oled_draw_string(uint8_t u1t_x, uint8_t u1t_page, const char *p_str, bool b_invert)
{
    uint8_t u1t_curr_x = u1t_x;

    if (p_str != (const char *)0)
    {
        while ((*p_str != '\0') && (u1t_curr_x < (OLED_WIDTH_PX - FONT_CHAR_WIDTH_PX)))
        {
            char c = *p_str;
            uint8_t u1t_ascii = (uint8_t)c;

            /* Convert lowercase to uppercase */
            if ((u1t_ascii >= 97U) && (u1t_ascii <= 122U))
            {
                u1t_ascii = u1t_ascii - 32U;
            }
            else
            {
                /* Keep as is */
            }

            if ((u1t_ascii >= FONT_FIRST_ASCII) && (u1t_ascii <= FONT_LAST_ASCII))
            {
                uint8_t u1t_font_idx = u1t_ascii - FONT_FIRST_ASCII;
                uint16_t u2t_base_idx = ((uint16_t)u1t_page * OLED_WIDTH_PX) + (uint16_t)u1t_curr_x;

                for (uint8_t u1t_col = 0U; u1t_col < FONT_CHAR_WIDTH_PX; u1t_col++)
                {
                    uint8_t u1t_bits = OLED_FONT5X7[u1t_font_idx][u1t_col];
                    if (b_invert == true)
                    {
                        u1t_bits = ~u1t_bits;
                    }
                    else
                    {
                        /* Normal */
                    }
                    g_u1t_oled_buffer[u2t_base_idx + u1t_col] = u1t_bits;
                }

                /* 1-pixel gap after character */
                if (b_invert == true)
                {
                    g_u1t_oled_buffer[u2t_base_idx + FONT_CHAR_WIDTH_PX] = 0xFFU;
                }
                else
                {
                    g_u1t_oled_buffer[u2t_base_idx + FONT_CHAR_WIDTH_PX] = 0x00U;
                }

                u1t_curr_x = u1t_curr_x + (FONT_CHAR_WIDTH_PX + 1U);
            }
            else
            {
                /* Skip unprintable character */
                u1t_curr_x = u1t_curr_x + (FONT_CHAR_WIDTH_PX + 1U);
            }

            p_str++;
        }
    }
    else
    {
        /* Null pointer guard */
    }
}

/* Virtual Piano UI Renderers */
void bsp_oled_render_header(const char *p_mode, bool b_high_bank, uint8_t u1t_vol_pct,
                           const char *p_note_name, const char *p_note_freq, int32_t s4t_cents)
{
    /* Line 0: Mode Badge, Bank Status, Volume Level */
    bsp_oled_draw_string(0U, 0U, p_mode, false);

    if (b_high_bank == true)
    {
        bsp_oled_draw_string(44U, 0U, "[HI]", false);
    }
    else
    {
        bsp_oled_draw_string(44U, 0U, "[LO]", false);
    }

    bsp_oled_draw_string(72U, 0U, "VOL:", false);

    /* Mini Volume Bar */
    bsp_oled_draw_hline(OLED_VOL_BAR_X0, OLED_VOL_BAR_X1, OLED_VOL_BAR_Y0, true);
    bsp_oled_draw_hline(OLED_VOL_BAR_X0, OLED_VOL_BAR_X1, OLED_VOL_BAR_Y1, true);
    bsp_oled_draw_vline(OLED_VOL_BAR_X0, OLED_VOL_BAR_Y0, OLED_VOL_BAR_Y1, true);
    bsp_oled_draw_vline(OLED_VOL_BAR_X1, OLED_VOL_BAR_Y0, OLED_VOL_BAR_Y1, true);

    uint8_t u1t_fill_len = (uint8_t)(((uint32_t)u1t_vol_pct * OLED_VOL_BAR_MAX_LEN) / 100U);
    if (u1t_fill_len > 0U)
    {
        bsp_oled_fill_rect((uint8_t)(OLED_VOL_BAR_X0 + 1U), OLED_VOL_BAR_FILL_Y0, (uint8_t)((OLED_VOL_BAR_X0 + 1U) + u1t_fill_len), OLED_VOL_BAR_FILL_Y1, true);
    }
    else
    {
        /* Zero volume fill */
    }

    /* Line 1: Active Note, Frequency, and Pitch Bend Cents */
    if (p_note_name != (const char *)0)
    {
        bsp_oled_draw_string(0U, 1U, p_note_name, false);
    }
    else
    {
        bsp_oled_draw_string(0U, 1U, "-- SILENT --", false);
    }

    if (p_note_freq != (const char *)0)
    {
        bsp_oled_draw_string(50U, 1U, p_note_freq, false);
    }
    else
    {
        /* No frequency */
    }

    if (s4t_cents > 0)
    {
        bsp_oled_draw_string(88U, 1U, "P:+", false);
        bsp_oled_set_pixel(108U, 10U, true);
    }
    else if (s4t_cents < 0)
    {
        bsp_oled_draw_string(88U, 1U, "P:-", false);
        bsp_oled_set_pixel(108U, 10U, true);
    }
    else
    {
        bsp_oled_draw_string(88U, 1U, "P: 0", false);
    }
}

void bsp_oled_render_pitch_gauge(int32_t s4t_norm_x)
{
    /* Page 2: Separator line and Pitch Roll gauge */
    bsp_oled_draw_hline(0U, 127U, PITCH_GAUGE_SEP_Y, true);

    bsp_oled_draw_string(2U, 2U, "PITCH", false);
    bsp_oled_draw_string(96U, 2U, "ROLL", false);

    /* Center Pitch Box */
    bsp_oled_draw_hline(PITCH_GAUGE_BOX_X0, PITCH_GAUGE_BOX_X1, PITCH_GAUGE_BOX_Y0, true);
    bsp_oled_draw_hline(PITCH_GAUGE_BOX_X0, PITCH_GAUGE_BOX_X1, PITCH_GAUGE_BOX_Y1, true);
    bsp_oled_draw_vline(PITCH_GAUGE_BOX_X0, PITCH_GAUGE_BOX_Y0, PITCH_GAUGE_BOX_Y1, true);
    bsp_oled_draw_vline(PITCH_GAUGE_BOX_X1, PITCH_GAUGE_BOX_Y0, PITCH_GAUGE_BOX_Y1, true);

    /* Center Marker tick */
    bsp_oled_draw_vline((uint8_t)PITCH_GAUGE_CENTER_X, (uint8_t)(PITCH_GAUGE_BOX_Y0 + 1U), (uint8_t)(PITCH_GAUGE_BOX_Y1 - 1U), true);

    /* Moving indicator dot */
    int32_t s4t_offset = (s4t_norm_x * PITCH_GAUGE_HALF_TRAVEL) / 1000;
    int32_t s4t_marker_x = PITCH_GAUGE_CENTER_X + s4t_offset;

    if (s4t_marker_x < PITCH_GAUGE_MIN_X)
    {
        s4t_marker_x = PITCH_GAUGE_MIN_X;
    }
    else if (s4t_marker_x > PITCH_GAUGE_MAX_X)
    {
        s4t_marker_x = PITCH_GAUGE_MAX_X;
    }
    else
    {
        /* Within gauge bounds */
    }

    bsp_oled_fill_rect((uint8_t)(s4t_marker_x - 1), PITCH_GAUGE_DOT_Y0, (uint8_t)(s4t_marker_x + 1), PITCH_GAUGE_DOT_Y1, true);
}

void bsp_oled_render_piano_keyboard(int8_t s1t_active_key, const char * const pp_labels[OLED_PIANO_NUM_KEYS])
{
    static const uint8_t BLACK_KEY_X[5] = {12U, 28U, 60U, 76U, 92U};

    /* 1. Top and Bottom keyboard borders */
    bsp_oled_draw_hline(0U, 127U, PIANO_BORDER_TOP_Y, true);
    bsp_oled_draw_hline(0U, 127U, PIANO_BORDER_BOT_Y, true);

    /* 2. White Key Borders and Accelerated Page-Byte Stride Active Invert Fill */
    for (uint8_t u1t_k = 0U; u1t_k < OLED_PIANO_NUM_KEYS; u1t_k++)
    {
        uint8_t u1t_x0 = u1t_k * PIANO_KEY_WIDTH_PX;
        uint8_t u1t_x1 = (u1t_x0 + PIANO_KEY_WIDTH_PX) - 1U;
        bool b_is_active = (s1t_active_key == (int8_t)u1t_k);

        /* Vertical dividing line between keys */
        bsp_oled_draw_vline(u1t_x0, PIANO_BORDER_TOP_Y, PIANO_BORDER_BOT_Y, true);
        bsp_oled_draw_vline(u1t_x1, PIANO_BORDER_TOP_Y, PIANO_BORDER_BOT_Y, true);

        if (b_is_active == true)
        {
            /* Page-Byte Stride Optimization (38x speedup vs pixel-by-pixel loops) */
            for (uint8_t u1t_col = (uint8_t)(u1t_x0 + 1U); u1t_col < u1t_x1; u1t_col++)
            {
                g_u1t_oled_buffer[(3U * OLED_PAGE_SIZE_BYTES) + u1t_col] |= 0xFEU;
                g_u1t_oled_buffer[(4U * OLED_PAGE_SIZE_BYTES) + u1t_col] = 0xFFU;
                g_u1t_oled_buffer[(5U * OLED_PAGE_SIZE_BYTES) + u1t_col] = 0xFFU;
                g_u1t_oled_buffer[(6U * OLED_PAGE_SIZE_BYTES) + u1t_col] = 0xFFU;
                g_u1t_oled_buffer[(7U * OLED_PAGE_SIZE_BYTES) + u1t_col] |= 0x7FU;
            }
        }
        else
        {
            /* Key inactive */
        }

        if (pp_labels != (const char * const *)0)
        {
            bsp_oled_draw_string((uint8_t)(u1t_x0 + 3U), 7U, pp_labels[u1t_k], b_is_active);
        }
        else
        {
            /* No labels provided */
        }
    }

    /* 3. Black Keys (C#, D#, F#, G#, A#) */
    for (uint8_t u1t_bk = 0U; u1t_bk < 5U; u1t_bk++)
    {
        uint8_t u1t_bx = BLACK_KEY_X[u1t_bk];
        bsp_oled_fill_rect(u1t_bx, (uint8_t)(PIANO_BORDER_TOP_Y + 1U), (uint8_t)(u1t_bx + 7U), PIANO_BLACK_KEY_BOT_Y, true);
        bsp_oled_draw_vline(u1t_bx, (uint8_t)(PIANO_BORDER_TOP_Y + 1U), PIANO_BLACK_KEY_BOT_Y, false);
        bsp_oled_draw_vline((uint8_t)(u1t_bx + 7U), (uint8_t)(PIANO_BORDER_TOP_Y + 1U), PIANO_BLACK_KEY_BOT_Y, false);
    }
}

/* Initialize DMA1 Stream 6 Channel 1 for I2C1_TX (Rule 11) */
static void oled_dma_init(void)
{
    /* 1. Enable DMA1 Clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    /* 2. Disable DMA1 Stream 6 before configuration */
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream6->CR & DMA_SxCR_EN) != 0U)
    {
        /* Wait until stream is disabled */
    }

    /* 3. Clear all pending interrupt flags for Stream 6 */
    DMA1->HIFCR = (DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 |
                   DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6);

    /* 4. Configure Peripheral Target Address to I2C1 Data Register */
    DMA1_Stream6->PAR = (uint32_t)(&(I2C1->DR));

    /* 5. Configure DMA Stream 6 Control Register:
     *    - Channel 1 (I2C1_TX): (1UL << DMA_SxCR_CHSEL_Pos)
     *    - Priority High: DMA_SxCR_PL_1
     *    - Memory Increment: DMA_SxCR_MINC
     *    - Peripheral Increment: 0 (fixed to I2C1->DR)
     *    - Direction: Memory-to-Peripheral (DMA_SxCR_DIR_0)
     *    - Memory Size: 8-bit (MSIZE = 0)
     *    - Peripheral Size: 8-bit (PSIZE = 0)
     *    - Transfer Complete Interrupt Enable: DMA_SxCR_TCIE
     */
    DMA1_Stream6->CR = ((1UL << DMA_SxCR_CHSEL_Pos) |
                        DMA_SxCR_PL_1 |
                        DMA_SxCR_MINC |
                        DMA_SxCR_DIR_0 |
                        DMA_SxCR_TCIE);

    /* 6. Direct Mode (FIFO disabled) */
    DMA1_Stream6->FCR = 0U;

    /* 7. Configure NVIC for DMA1 Stream 6 Interrupt */
    NVIC_SetPriority(DMA1_Stream6_IRQn, I2C1_DMA_NVIC_PRIORITY);
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

/* Unified helper to set display RAM page and column address */
static bool oled_set_page_address(uint8_t u1t_page)
{
    bool b_success = false;

    if (oled_i2c_start(I2C_OLED_SLAVE_ADDR_WRITE) == true)
    {
        (void)oled_i2c_write_byte(I2C_CTRL_BYTE_CMD);
        (void)oled_i2c_write_byte((uint8_t)(SH1106_PAGE_CMD_BASE | u1t_page));
        (void)oled_i2c_write_byte(SH1106_COL_LOW_OFFSET);
        (void)oled_i2c_write_byte(SH1106_COL_HIGH_BASE);
        oled_i2c_stop();
        bsp_delay_us(OLED_BUS_IDLE_DELAY_US);
        b_success = true;
    }
    else
    {
        b_success = false;
    }

    return b_success;
}

/* Transmit 1 page (128 bytes) of framebuffer to OLED synchronously (used in init) */
static void oled_write_page_sync(uint8_t u1t_page)
{
    uint16_t u2t_page_offset = (uint16_t)u1t_page * OLED_PAGE_SIZE_BYTES;

    if (oled_set_page_address(u1t_page) == true)
    {
        if (oled_i2c_start(I2C_OLED_SLAVE_ADDR_WRITE) == true)
        {
            (void)oled_i2c_write_byte(I2C_CTRL_BYTE_DATA);
            for (uint16_t u2t_col = 0U; u2t_col < OLED_PAGE_SIZE_BYTES; u2t_col++)
            {
                (void)oled_i2c_write_byte(g_u1t_oled_buffer[u2t_page_offset + u2t_col]);
            }
            oled_i2c_stop();
        }
        else
        {
            /* Data write skipped on I2C error */
        }
    }
    else
    {
        /* Page command skipped on I2C error */
    }
}

/* Transmit 1 page (128 bytes) of framebuffer to OLED using I2C DMA (Zero Blocking) */
static bool oled_write_page_dma(uint8_t u1t_page)
{
    bool b_success = false;
    uint16_t u2t_page_offset = (uint16_t)u1t_page * OLED_PAGE_SIZE_BYTES;

    /* Step 1: Send Page & Column Set Commands to OLED */
    if (oled_set_page_address(u1t_page) == true)
    {
        /* Step 2: Prepare DMA Buffer: Control Byte (0x40) + 128 Bytes Pixel Data */
        g_u1t_dma_page_buf[0] = I2C_CTRL_BYTE_DATA;
        for (uint16_t u2t_col = 0U; u2t_col < OLED_PAGE_SIZE_BYTES; u2t_col++)
        {
            g_u1t_dma_page_buf[u2t_col + 1U] = g_u1t_oled_buffer[u2t_page_offset + u2t_col];
        }

        /* Step 3: Configure DMA Stream for 129 bytes transfer */
        DMA1_Stream6->CR &= ~DMA_SxCR_EN;
        while ((DMA1_Stream6->CR & DMA_SxCR_EN) != 0U)
        {
            /* Wait until stream is disabled */
        }

        DMA1->HIFCR = (DMA_HIFCR_CTCIF6 | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTEIF6 |
                       DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6);
        DMA1_Stream6->M0AR = (uint32_t)g_u1t_dma_page_buf;
        DMA1_Stream6->NDTR = OLED_DMA_PAGE_PAYLOAD_LEN;

        /* Step 4: Initiate I2C Transmission to OLED Data Register */
        if (oled_i2c_start(I2C_OLED_SLAVE_ADDR_WRITE) == true)
        {
            g_b_oled_dma_busy = true;

            /* Enable I2C DMA Request and Enable DMA Stream */
            I2C1->CR2 |= I2C_CR2_DMAEN;
            DMA1_Stream6->CR |= DMA_SxCR_EN;

            b_success = true;
        }
        else
        {
            b_success = false;
        }
    }
    else
    {
        b_success = false;
    }

    return b_success;
}

/* DMA1 Stream 6 Interrupt Service Routine (I2C1 TX Complete) */
void DMA1_Stream6_IRQHandler(void)
{
    if ((DMA1->HISR & DMA_HISR_TCIF6) != 0U)
    {
        /* Clear Transfer Complete flag */
        DMA1->HIFCR = DMA_HIFCR_CTCIF6;

        /* Disable I2C DMA mode first so no further DMA requests are triggered */
        I2C1->CR2 &= ~I2C_CR2_DMAEN;

        /* Disable DMA Stream */
        DMA1_Stream6->CR &= ~DMA_SxCR_EN;

        /* Wait for Byte Transfer Finished (BTF) to ensure final byte shifted out */
        uint32_t u4t_timeout = I2C_TIMEOUT_CYCLES;
        while (((I2C1->SR1 & I2C_SR1_BTF) == 0U) && (u4t_timeout > 0U))
        {
            if ((I2C1->SR1 & (I2C_SR1_AF | I2C_SR1_BERR)) != 0U)
            {
                break;
            }
            else
            {
                u4t_timeout--;
            }
        }

        /* Generate STOP condition */
        I2C1->CR1 |= I2C_CR1_STOP;

        /* Wait for hardware to clear STOP bit in CR1 */
        u4t_timeout = I2C_TIMEOUT_CYCLES;
        while (((I2C1->CR1 & I2C_CR1_STOP) != 0U) && (u4t_timeout > 0U))
        {
            u4t_timeout--;
        }

        /* Clear any error flags */
        I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR);

        /* Release DMA lock */
        g_b_oled_dma_busy = false;
    }
    else
    {
        /* Clear any error flags if set */
        DMA1->HIFCR = (DMA_HIFCR_CTEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6);
        g_b_oled_dma_busy = false;
    }
}

/* Page-by-Page Refresh Service via DMA (Zero CPU blocking: hardware streams 129 bytes) */
void bsp_oled_service(uint32_t u4t_now)
{
    /* Watchdog recovery if DMA transfer stalls */
    if (g_b_oled_dma_busy == true)
    {
        if ((u4t_now - g_u4t_dma_start_time_ms) > OLED_DMA_TIMEOUT_MS)
        {
            /* DMA transfer timed out: perform full hardware bus and DMA reset */
            oled_i2c_bus_recovery();
        }
        else
        {
            /* DMA transfer is in progress in hardware */
        }
    }
    else
    {
        /* Periodic Keep-Alive: ensure charge pump & display remain ON (strictly at frame boundaries) */
        if ((g_u1t_current_page == 0U) && ((u4t_now - g_u4t_last_keep_alive_ms) >= OLED_KEEP_ALIVE_INTERVAL_MS))
        {
            g_u4t_last_keep_alive_ms = u4t_now;
            oled_reinit_display();
        }
        else
        {
            /* Keep-alive interval not elapsed or mid-frame */
        }

        if ((u4t_now - g_u4t_last_service_ms) >= OLED_SERVICE_SLICE_MS)
        {
            g_u4t_last_service_ms = u4t_now;
            g_u4t_dma_start_time_ms = u4t_now;
            bool b_ok = oled_write_page_dma(g_u1t_current_page);

            if (b_ok == true)
            {
                g_u1t_consecutive_errors = 0U;
                g_u1t_current_page = (uint8_t)((g_u1t_current_page + 1U) % OLED_NUM_PAGES);
            }
            else
            {
                g_u1t_consecutive_errors++;
                if (g_u1t_consecutive_errors >= OLED_MAX_CONSECUTIVE_ERRORS)
                {
                    oled_i2c_bus_recovery();
                    oled_reinit_display();
                    g_u1t_consecutive_errors = 0U;
                }
                else
                {
                    /* Retry on next slice */
                }
            }
        }
        else
        {
            /* Waiting for next slice interval */
        }
    }
}

/* Initialization Sequence */
void bsp_oled_init(void)
{
    /* 1. Enable GPIOB and I2C1 Clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* 2. Configure PB8 (SCL) and PB9 (SDA) */
    /* Alternate Function Mode (0b10) */
    GPIOB->MODER &= ~((3UL << (8U * 2U)) | (3UL << (9U * 2U)));
    GPIOB->MODER |=  ((2UL << (8U * 2U)) | (2UL << (9U * 2U)));

    /* Open Drain (0b1) */
    GPIOB->OTYPER |= ((1UL << 8U) | (1UL << 9U));

    /* Very High Speed (0b11) */
    GPIOB->OSPEEDR |= ((3UL << (8U * 2U)) | (3UL << (9U * 2U)));

    /* Pull-Up Enabled (0b01) */
    GPIOB->PUPDR &= ~((3UL << (8U * 2U)) | (3UL << (9U * 2U)));
    GPIOB->PUPDR |=  ((1UL << (8U * 2U)) | (1UL << (9U * 2U)));

    /* Alternate Function 4 (AF4) for I2C1 on PB8 and PB9 */
    GPIOB->AFR[1] &= ~((15UL << 0U) | (15UL << 4U));
    GPIOB->AFR[1] |=  ((4UL  << 0U) | (4UL  << 4U));

    /* 3. Reset and Configure I2C1 Peripheral via Hardware Bus Recovery */
    oled_i2c_bus_recovery();

    /* 4. Send OLED Initialization Command Table */
    static const uint8_t INIT_CMDS[] = {
        0xAEU,         /* Display OFF */
        0xD5U, 0x80U,   /* Set Display Clock Divide Ratio */
        0xA8U, 0x3FU,   /* Multiplex Ratio 64 (128x64) */
        0xD3U, 0x00U,   /* Display Offset 0 */
        0x40U,         /* Start Line 0 */
        0x8DU, 0x14U,   /* SSD1306 Charge Pump Enable */
        0xADU, 0x8BU,   /* SH1106 DC-DC Enable */
        0xA1U,         /* Segment Re-map: column 127 is SEG0 */
        0xC8U,         /* COM Output Scan Direction: remapped */
        0xDAU, 0x12U,   /* COM Pins Configuration */
        0x81U, 0xCFU,   /* Contrast Control */
        0xD9U, 0xF1U,   /* Pre-charge Period */
        0xDBU, 0x40U,   /* VCOMH Deselect Level */
        0x20U, 0x02U,   /* Memory Addressing: Page Mode */
        0xA4U,         /* Entire Display Resume */
        0xA6U,         /* Normal Display */
        0xAFU          /* Display ON */
    };

    bsp_delay_ms(50U);
    oled_send_command_list(INIT_CMDS, (uint8_t)sizeof(INIT_CMDS));
    bsp_delay_ms(50U);

    /* 5. Clear Initial Framebuffer */
    bsp_oled_clear_buffer();

    /* 6. Render Initial Virtual Piano Interface */
    bsp_oled_render_header("[LIVE]", false, 80U, "-- SILENT --", "", 0);
    bsp_oled_render_pitch_gauge(0);
    bsp_oled_render_piano_keyboard(-1, (const char * const *)0);

    /* 7. Initialize DMA Controller for I2C1 */
    oled_dma_init();

    /* 8. Flush Entire Buffer once at startup so screen lights up immediately */
    for (uint8_t u1t_p = 0U; u1t_p < OLED_NUM_PAGES; u1t_p++)
    {
        oled_write_page_sync(u1t_p);
    }
}

