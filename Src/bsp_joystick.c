/*******************************************************************************
 * File Name   : bsp_joystick.c
 * Description : HW-504 Joystick - VRx (PC0) / VRy (PC1) normalized to -1000..+1000,
 *               SW (PC2): EXTI2 edge -> 30 ms debounce -> Short / Long press events
 * Target MCU  : STM32F411RET6
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "bsp_joystick.h"
#include "bsp_adc.h"
#include "bsp_gpio.h"

/* Named Constants (Rule 5 & Rule 10) */
#define JOY_ADC_MAX             (4095)
#define JOY_CENTER_VAL          (2048)
#define JOY_DEADZONE_COUNTS     (160)
#define JOY_NORM_MAX            (1000)
#define JOY_SPAN_POS            (JOY_ADC_MAX - JOY_CENTER_VAL - JOY_DEADZONE_COUNTS)    /* Counts above the deadzone */
#define JOY_SPAN_NEG            (JOY_CENTER_VAL - JOY_DEADZONE_COUNTS)                  /* Counts below the deadzone */
#define JOY_EMA_WEIGHT_PREV     (3U)
#define JOY_EMA_WEIGHT_DIV      (4U)
#define SW_DEBOUNCE_MS          (30U)
#define SW_HOLD_MS              (600U)

static uint32_t         g_u4t_ema_x = (uint32_t)JOY_CENTER_VAL;
static uint32_t         g_u4t_ema_y = (uint32_t)JOY_CENTER_VAL;

/* Switch starts "pressed + long already fired": no event until a full release/press cycle,
 * so a switch held (or stuck) at boot can never trigger a command.
 * The edge flag starts set so the real level is read once, 30 ms after boot. */
static bool             g_b_sw_edge_pending = true;
static bool             g_b_sw_pressed = true;
static bool             g_b_sw_long_fired = true;
static uint32_t         g_u4t_sw_edge_ms = 0U;
static uint32_t         g_u4t_sw_press_ms = 0U;
static joy_sw_event_t   g_sw_event = JOY_SW_EVT_NONE;

/* Deadzone around center, then scale each half to -1000..+1000 */
static int32_t joystick_calc_norm(uint32_t u4t_ema)
{
    int32_t s4t_diff = (int32_t)u4t_ema - JOY_CENTER_VAL;
    int32_t s4t_norm = 0;

    if (s4t_diff > JOY_DEADZONE_COUNTS)
    {
        s4t_norm = ((s4t_diff - JOY_DEADZONE_COUNTS) * JOY_NORM_MAX) / JOY_SPAN_POS;
    }
    else if (s4t_diff < -JOY_DEADZONE_COUNTS)
    {
        s4t_norm = ((s4t_diff + JOY_DEADZONE_COUNTS) * JOY_NORM_MAX) / JOY_SPAN_NEG;
    }
    else
    {
        s4t_norm = 0;
    }
    return s4t_norm;
}

void bsp_joystick_service(uint32_t u4t_now)
{
    /* Axes: EMA filter (3/4 old + 1/4 new) on DMA-fed ADC samples */
    g_u4t_ema_x = ((g_u4t_ema_x * JOY_EMA_WEIGHT_PREV) + bsp_adc_get_raw(ADC_IDX_JOY_X)) / JOY_EMA_WEIGHT_DIV;
    g_u4t_ema_y = ((g_u4t_ema_y * JOY_EMA_WEIGHT_PREV) + bsp_adc_get_raw(ADC_IDX_JOY_Y)) / JOY_EMA_WEIGHT_DIV;

    /* Switch: EXTI2 reports every edge on PC2; each edge (bounce included) restarts the 30 ms window */
    if (bsp_gpio_get_exti_flag() == true)
    {
        bsp_gpio_clear_exti_flag();
        g_b_sw_edge_pending = true;
        g_u4t_sw_edge_ms = u4t_now;
    }
    else
    {
        /* No edge since the last pass */
    }

    /* No edge for 30 ms: the contact has settled, read the final level once */
    if ((g_b_sw_edge_pending == true) && ((u4t_now - g_u4t_sw_edge_ms) >= SW_DEBOUNCE_MS))
    {
        bool b_level = bsp_gpio_read_joystick_switch();

        g_b_sw_edge_pending = false;
        if (b_level != g_b_sw_pressed)
        {
            g_b_sw_pressed = b_level;
            if (b_level == true)
            {
                g_u4t_sw_press_ms = u4t_now;
                g_b_sw_long_fired = false;
            }
            else if (g_b_sw_long_fired == false)
            {
                g_sw_event = JOY_SW_EVT_SHORT_PRESS;
            }
            else
            {
                /* Release after long press: already reported */
            }
        }
        else
        {
            /* Bounce ended at the same level: no change */
        }
    }
    else
    {
        /* No pending edge, or still inside the debounce window */
    }

    if ((g_b_sw_pressed == true) && (g_b_sw_long_fired == false) && ((u4t_now - g_u4t_sw_press_ms) >= SW_HOLD_MS))
    {
        g_b_sw_long_fired = true;
        g_sw_event = JOY_SW_EVT_LONG_PRESS;
    }
    else
    {
        /* No action required */
    }
}

int32_t bsp_joystick_get_norm_x(void)
{
    return joystick_calc_norm(g_u4t_ema_x);
}

int32_t bsp_joystick_get_norm_y(void)
{
    return joystick_calc_norm(g_u4t_ema_y);
}

/* Returns the pending switch event and clears it */
joy_sw_event_t bsp_joystick_get_event(void)
{
    joy_sw_event_t evt = g_sw_event;
    g_sw_event = JOY_SW_EVT_NONE;
    return evt;
}
