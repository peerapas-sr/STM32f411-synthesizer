/*******************************************************************************
 * File Name   : bsp_joystick.h
 * Description : HW-504 Joystick - VRx (PC0), VRy (PC1), SW (PC2)
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#ifndef BSP_JOYSTICK_H
#define BSP_JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    JOY_SW_EVT_NONE = 0,
    JOY_SW_EVT_SHORT_PRESS,
    JOY_SW_EVT_LONG_PRESS
} joy_sw_event_t;

void           bsp_joystick_service(uint32_t u4t_now);
int32_t        bsp_joystick_get_norm_x(void);
int32_t        bsp_joystick_get_norm_y(void);
joy_sw_event_t bsp_joystick_get_event(void);

#endif /* BSP_JOYSTICK_H */
