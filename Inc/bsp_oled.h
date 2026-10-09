/*******************************************************************************
 * File Name   : bsp_oled.h
 * Description : 1.30" SH1106 I2C OLED - SCL PB8, SDA PB9 (I2C1 + DMA1 Stream 6)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdint.h>
#include <stdbool.h>

#define OLED_WIDTH_PX               (128U)
#define OLED_NUM_PAGES              (8U)
#define OLED_PIANO_NUM_KEYS         (8U)

void bsp_oled_init(void);
void bsp_oled_service(uint32_t u4t_now);
void bsp_oled_clear_buffer(void);

/* Virtual Piano UI Renderers (draw into framebuffer; bsp_oled_service streams it) */
void bsp_oled_render_header(const char *p_mode, bool b_high_bank, uint8_t u1t_vol_pct,
                            const char *p_note_name, const char *p_note_freq, int32_t s4t_cents);
void bsp_oled_render_pitch_gauge(int32_t s4t_norm_x);
void bsp_oled_render_piano_keyboard(int8_t s1t_active_key, const char * const pp_labels[OLED_PIANO_NUM_KEYS]);

#endif /* BSP_OLED_H */
