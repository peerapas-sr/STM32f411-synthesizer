/*******************************************************************************
 * File Name   : app_synth.c
 * Description : Application Layer - Synthesizer, Sequence Recorder & Looping Playback
 * Standard    : Toyota Embedded MISRA-C Compliant (22 Rules)
 ******************************************************************************/

#include "app_synth.h"
#include "bsp_gpio.h"
#include "bsp_adc.h"
#include "bsp_joystick.h"
#include "bsp_uart.h"
#include "bsp_buzzer.h"
#include "bsp_timer.h"
#include "bsp_oled.h"

/* Named Constants (Rule 5 & Rule 10) */
#define SYNTH_MAX_STEPS             (64U)
#define SYNTH_NUM_NOTES             (8U)
#define SYNTH_NUM_KEYS              (4U)
#define SYNTH_NUM_COMBOS            (2U)
#define SYNTH_NOTE_SOL              (4U)
#define CHIME_NUM_NOTES             (3U)
#define SYNTH_HIGH_BANK_OFFSET      (4)
#define JOY_HIGH_BANK_THRESHOLD     (-350)
#define JOY_NORM_FULL               (1000)
#define OLED_RENDER_INTERVAL_MS     (30U)
#define KEY_COMBO_REC_MASK          (0x09U)    /* K1 + K4 held 600 ms: toggle recording */
#define KEY_COMBO_PLAY_MASK         (0x06U)    /* K2 + K3 held 600 ms: toggle playback */
#define KEY_LOCKOUT_MS              (20U)
#define COMBO_HOLD_MS               (600U)
#define COMBO_SKEW_WINDOW_MS        (700U)
#define UART_NOTE_DUR_MS            (300U)
#define NOTE_SUSTAIN_MS             (150U)
#define REC_LED_BLINK_MS            (200U)
#define TONE_C4_HZ                  (262U)
#define TONE_C6_HZ                  (1047U)
#define TONE_G6_HZ                  (1568U)
#define TONE_NONE_HZ                (0U)
#define TONE_VOL_RAW                (2500U)
#define TONE_STEP_MS                (50U)
#define PLAY_MIN_GAP_MS             (40U)
#define REC_MIN_NOTE_MS             (50U)
#define REC_MAX_NOTE_MS             (65535U)
#define REC_MAX_REST_MS             (3000U)
#define REC_DEF_REST_MS             (80U)
#define REC_TRANSITION_REST_MS      (30U)
#define VOLUME_RAW_MAX              (4095U)
#define PERCENT_SCALE               (100U)
#define BEND_MAX_CENTS              (200)
#define VIBRATO_MAX_CENTS           (50)
#define TOTAL_CENTS_CLAMP           (250)
#define LFO_PERIOD_MS               (200U)
#define LFO_HALF_MS                 (100)
#define LFO_QUARTER_MS              (50)
#define PITCH_TABLE_STEP_CENTS      (25U)
#define PITCH_TABLE_LAST_IDX        (20U)
#define PITCH_Q12_SHIFT             (12U)

typedef enum {
    MODE_LIVE = 0,
    MODE_RECORDING,
    MODE_PLAYING
} synth_mode_t;

typedef struct {
    int8_t   note_index;
    uint16_t duration_ms;
    uint16_t rest_ms;
} synth_step_t;

/* Note Tables (C7..C8 diatonic) and 21-point Q12 pitch ratios for -250..+250 cents */
static const uint32_t NOTE_FREQ[SYNTH_NUM_NOTES] = {2093U, 2349U, 2637U, 2794U, 3136U, 3520U, 3951U, 4186U};
static const char *NOTE_NAMES[SYNTH_NUM_NOTES] = {"C7 (DO)", "D7 (RE)", "E7 (MI)", "F7 (FA)",
                                                  "G7 (SO)", "A7 (LA)", "B7 (TI)", "C8 (DO)"};
static const char *NOTE_FREQS[SYNTH_NUM_NOTES] = {"2093H", "2349H", "2637H", "2794H", "3136H", "3520H", "3951H", "4186H"};
static const char *KEY_LABELS[SYNTH_NUM_NOTES] = {"DO", "RE", "MI", "FA", "SO", "LA", "TI", "C8"};
static const uint16_t PITCH_RATIO_Q12[PITCH_TABLE_LAST_IDX + 1U] = {
    3545U, 3597U, 3649U, 3702U, 3756U, 3811U, 3866U, 3922U, 3979U, 4037U, 4096U,
    4156U, 4216U, 4277U, 4340U, 4403U, 4467U, 4532U, 4598U, 4664U, 4732U
};
static const uint8_t COMBO_MASK[SYNTH_NUM_COMBOS] = {KEY_COMBO_REC_MASK, KEY_COMBO_PLAY_MASK};

/* Sequencer / Recorder / Playback State (g_s1t_rec_note < 0: no note being recorded) */
static synth_step_t  g_sequence[SYNTH_MAX_STEPS];
static uint16_t      g_u2t_seq_count = 0U;
static synth_mode_t  g_mode = MODE_LIVE;
static int8_t        g_s1t_rec_note = -1;
static uint32_t      g_u4t_rec_note_start_ms = 0U;
static uint32_t      g_u4t_rec_last_release_ms = 0U;
static uint32_t      g_u4t_blink_ms = 0U;
static bool          g_b_blink_on = false;
static uint16_t      g_u2t_play_step = 0U;
static uint32_t      g_u4t_play_step_start_ms = 0U;
static bool          g_b_play_in_note = false;

/* Live Input State */
static int8_t        g_s1t_last_played = -1;
static uint32_t      g_u4t_lfo_start_ms = 0U;
static int8_t        g_s1t_uart_note = -1;
static uint32_t      g_u4t_uart_note_ms = 0U;
static int8_t        g_s1t_sustain_note = -1;
static uint32_t      g_u4t_sustain_ms = 0U;
static uint8_t       g_u1t_keys = 0U;
static uint32_t      g_u4t_key_lockout_ms[SYNTH_NUM_KEYS] = {0U, 0U, 0U, 0U};
static uint32_t      g_u4t_combo_start_ms[SYNTH_NUM_COMBOS] = {0U, 0U};
static bool          g_b_combo_taken[SYNTH_NUM_COMBOS] = {false, false};

/* Note telemetry, same format in every mode: "<prefix>C7 (DO) (2093 Hz[, 300 ms])"; 0 ms = duration unknown */
static void synth_log_note(const char *p_prefix, int8_t s1t_note, uint32_t u4t_dur_ms)
{
    bsp_uart_send_string(p_prefix);
    bsp_uart_send_string(NOTE_NAMES[s1t_note]);
    bsp_uart_send_string(" (");
    bsp_uart_send_dec(NOTE_FREQ[s1t_note]);
    bsp_uart_send_string(" Hz");
    if (u4t_dur_ms > 0U)
    {
        bsp_uart_send_string(", ");
        bsp_uart_send_dec(u4t_dur_ms);
        bsp_uart_send_string(" ms");
    }
    bsp_uart_send_string(")\r\n");
}

/* One or two short status beeps (blocking 50 ms each) */
static void synth_beep(uint32_t u4t_first_hz, uint32_t u4t_second_hz)
{
    bsp_buzzer_set_tone(u4t_first_hz, TONE_VOL_RAW);
    bsp_delay_ms(TONE_STEP_MS);
    if (u4t_second_hz != TONE_NONE_HZ)
    {
        bsp_buzzer_set_tone(u4t_second_hz, TONE_VOL_RAW);
        bsp_delay_ms(TONE_STEP_MS);
    }
    bsp_buzzer_off();
}

/* Drive buzzer at frequency with volume percent and light the LED */
static void synth_sound(uint32_t u4t_freq, uint8_t u1t_vol_pct)
{
    bsp_gpio_led_red_set(true);
    bsp_buzzer_set_tone(u4t_freq, (uint16_t)(((uint32_t)u1t_vol_pct * VOLUME_RAW_MAX) / PERCENT_SCALE));
}

/* Recorder: close the sounding note and append it as a sequence step */
static void synth_rec_close_note(uint32_t u4t_now, uint16_t u2t_rest_ms)
{
    uint32_t u4t_dur = u4t_now - g_u4t_rec_note_start_ms;

    if (u4t_dur < REC_MIN_NOTE_MS)
    {
        u4t_dur = REC_MIN_NOTE_MS;
    }
    if (u4t_dur > REC_MAX_NOTE_MS)
    {
        u4t_dur = REC_MAX_NOTE_MS;
    }
    if ((g_s1t_rec_note >= 0) && (g_u2t_seq_count < SYNTH_MAX_STEPS))
    {
        g_sequence[g_u2t_seq_count].note_index = g_s1t_rec_note;
        g_sequence[g_u2t_seq_count].duration_ms = (uint16_t)u4t_dur;
        g_sequence[g_u2t_seq_count].rest_ms = u2t_rest_ms;
        g_u2t_seq_count++;
        synth_log_note("[RECORDER] Recorded: ", g_s1t_rec_note, u4t_dur);
    }
    g_s1t_rec_note = -1;
}

/* Unified Mode Transition: leave the current mode cleanly, then enter the new one */
static void synth_set_mode(synth_mode_t new_mode, uint32_t u4t_now)
{
    g_s1t_sustain_note = -1;
    bsp_buzzer_off();
    bsp_gpio_led_red_set(false);
    if (g_mode == MODE_RECORDING)
    {
        synth_rec_close_note(u4t_now, REC_DEF_REST_MS);
        synth_beep(TONE_G6_HZ, TONE_C6_HZ);
        bsp_uart_send_string("[RECORDER] Stopped & Saved! Total steps: ");
        bsp_uart_send_dec((uint32_t)g_u2t_seq_count);
        bsp_uart_send_string("\r\n");
    }
    else if (g_mode == MODE_PLAYING)
    {
        bsp_uart_send_string("[PLAYBACK] Stopped!\r\n");
    }
    else
    {
        /* Leaving live mode: nothing to clean up */
    }
    g_mode = MODE_LIVE;

    if (new_mode == MODE_RECORDING)
    {
        g_mode = MODE_RECORDING;
        g_u2t_seq_count = 0U;
        g_s1t_rec_note = -1;
        g_u4t_blink_ms = u4t_now;
        g_b_blink_on = true;
        bsp_gpio_led_red_set(true);
        synth_beep(TONE_C6_HZ, TONE_G6_HZ);
        bsp_uart_send_string("[RECORDER] Recording Started!\r\n");
    }
    else if ((new_mode == MODE_PLAYING) && (g_u2t_seq_count == 0U))
    {
        synth_beep(TONE_C4_HZ, TONE_NONE_HZ);
        bsp_uart_send_string("[PLAYBACK] Memory empty! Record first.\r\n");
    }
    else if (new_mode == MODE_PLAYING)
    {
        synth_beep(NOTE_FREQ[SYNTH_NOTE_SOL], TONE_NONE_HZ);
        g_mode = MODE_PLAYING;
        g_u2t_play_step = 0U;
        g_b_play_in_note = true;
        g_u4t_play_step_start_ms = bsp_timer_get_ms();
        bsp_uart_send_string("[PLAYBACK] Looping ");
        bsp_uart_send_dec((uint32_t)g_u2t_seq_count);
        bsp_uart_send_string(" notes (Press K2+K3, Joy SW, or 'p' to stop)...\r\n");
        synth_log_note("[PLAYBACK] Playing: ", g_sequence[0].note_index, (uint32_t)g_sequence[0].duration_ms);
    }
    else
    {
        /* Back to live mode */
    }
}

/* Enter the given mode, or return to live mode if already in it */
static void synth_toggle_mode(synth_mode_t mode, uint32_t u4t_now)
{
    if (g_mode == mode)
    {
        synth_set_mode(MODE_LIVE, u4t_now);
    }
    else
    {
        synth_set_mode(mode, u4t_now);
    }
}

/* Debounce 4 keys: a key may change state at most once per 20 ms lockout window.
 * Then run the 600 ms hold combos; returns true while any combo is held (notes muted). */
static bool synth_scan_keys(uint32_t u4t_now)
{
    uint8_t u1t_changed = (uint8_t)(bsp_gpio_read_keys() ^ g_u1t_keys);
    bool b_combo_held = false;

    for (uint8_t u1t_i = 0U; u1t_i < SYNTH_NUM_KEYS; u1t_i++)
    {
        if (((u1t_changed & (1U << u1t_i)) != 0U) && ((u4t_now - g_u4t_key_lockout_ms[u1t_i]) >= KEY_LOCKOUT_MS))
        {
            g_u1t_keys ^= (uint8_t)(1U << u1t_i);
            g_u4t_key_lockout_ms[u1t_i] = u4t_now;
        }
    }

    for (uint8_t u1t_c = 0U; u1t_c < SYNTH_NUM_COMBOS; u1t_c++)
    {
        if ((g_u1t_keys & COMBO_MASK[u1t_c]) != COMBO_MASK[u1t_c])
        {
            g_u4t_combo_start_ms[u1t_c] = u4t_now;
            g_b_combo_taken[u1t_c] = false;
        }
        else if (((u4t_now - g_u4t_combo_start_ms[u1t_c]) >= COMBO_HOLD_MS) && (g_b_combo_taken[u1t_c] == false))
        {
            g_b_combo_taken[u1t_c] = true;
            if (u1t_c == 0U)
            {
                /* Drop the stray step recorded by the first finger to land on the combo */
                if ((g_mode == MODE_RECORDING) && (g_u2t_seq_count > 0U) &&
                    ((u4t_now - g_u4t_rec_last_release_ms) <= COMBO_SKEW_WINDOW_MS))
                {
                    g_u2t_seq_count--;
                }
                synth_toggle_mode(MODE_RECORDING, u4t_now);
            }
            else
            {
                synth_toggle_mode(MODE_PLAYING, u4t_now);
            }
            b_combo_held = true;
        }
        else
        {
            b_combo_held = true;
        }
    }
    return b_combo_held;
}

/* Active note: first pressed key (+4 in high bank), else UART trigger, else 150 ms sustain tail */
static int8_t synth_read_active_note(bool b_combo_held, int32_t s4t_norm_y, uint32_t u4t_now)
{
    int8_t s1t_note = -1;
    int8_t s1t_bank = 0;

    if (s4t_norm_y <= JOY_HIGH_BANK_THRESHOLD)
    {
        s1t_bank = SYNTH_HIGH_BANK_OFFSET;
    }
    for (uint8_t u1t_i = 0U; (u1t_i < SYNTH_NUM_KEYS) && (s1t_note < 0); u1t_i++)
    {
        if ((g_u1t_keys & (1U << u1t_i)) != 0U)
        {
            s1t_note = (int8_t)((int8_t)u1t_i + s1t_bank);
        }
    }
    if ((s1t_note < 0) && (g_s1t_uart_note >= 0) && ((u4t_now - g_u4t_uart_note_ms) < UART_NOTE_DUR_MS))
    {
        s1t_note = g_s1t_uart_note;
    }

    if (b_combo_held == true)
    {
        s1t_note = -1;
        g_s1t_sustain_note = -1;
    }
    else if (s1t_note >= 0)
    {
        g_s1t_sustain_note = s1t_note;
        g_u4t_sustain_ms = u4t_now;
    }
    else if ((g_s1t_sustain_note >= 0) && ((u4t_now - g_u4t_sustain_ms) < NOTE_SUSTAIN_MS))
    {
        s1t_note = g_s1t_sustain_note;
    }
    else
    {
        g_s1t_sustain_note = -1;
    }
    return s1t_note;
}

/* Note frequency with pitch bend (X) and triangle-LFO vibrato (Y > 0), via Q12 ratio table */
static uint32_t synth_modulated_freq(int8_t s1t_note, int32_t s4t_norm_x, int32_t s4t_norm_y, uint32_t u4t_now)
{
    int32_t s4t_vib_depth = 0;
    /* Triangle wave over 200 ms: 0 -> +50 -> 0 -> -50 -> 0, computed as 50 - |q - 100| */
    int32_t s4t_q = (int32_t)(((u4t_now - g_u4t_lfo_start_ms) + (uint32_t)LFO_QUARTER_MS) % LFO_PERIOD_MS) - LFO_HALF_MS;

    if (s4t_q < 0)
    {
        s4t_q = -s4t_q;
    }
    if (s4t_norm_y > 0)
    {
        s4t_vib_depth = (s4t_norm_y * VIBRATO_MAX_CENTS) / JOY_NORM_FULL;
    }

    int32_t s4t_cents = ((s4t_norm_x * BEND_MAX_CENTS) / JOY_NORM_FULL) +
                        (((LFO_QUARTER_MS - s4t_q) * s4t_vib_depth) / LFO_QUARTER_MS);
    if (s4t_cents > TOTAL_CENTS_CLAMP)
    {
        s4t_cents = TOTAL_CENTS_CLAMP;
    }
    if (s4t_cents < -TOTAL_CENTS_CLAMP)
    {
        s4t_cents = -TOTAL_CENTS_CLAMP;
    }

    /* Linear interpolation between 25-cent table points */
    uint32_t u4t_shift = (uint32_t)(s4t_cents + TOTAL_CENTS_CLAMP);
    uint32_t u4t_idx = u4t_shift / PITCH_TABLE_STEP_CENTS;
    uint32_t u4t_ratio = (uint32_t)PITCH_RATIO_Q12[u4t_idx];
    if (u4t_idx < PITCH_TABLE_LAST_IDX)
    {
        u4t_ratio += ((((uint32_t)PITCH_RATIO_Q12[u4t_idx + 1U] - u4t_ratio) * (u4t_shift % PITCH_TABLE_STEP_CENTS)) /
                      PITCH_TABLE_STEP_CENTS);
    }
    return (NOTE_FREQ[s1t_note] * u4t_ratio) >> PITCH_Q12_SHIFT;
}

/* Recording Mode: append a step on every note change / release, blink LED while idle */
static void synth_update_recording(uint32_t u4t_now, int8_t s1t_note)
{
    if ((s1t_note >= 0) && (s1t_note != g_s1t_rec_note))
    {
        if (g_s1t_rec_note >= 0)
        {
            synth_rec_close_note(u4t_now, REC_TRANSITION_REST_MS);
        }
        else if (g_u2t_seq_count > 0U)
        {
            uint32_t u4t_rest = u4t_now - g_u4t_rec_last_release_ms;
            if (u4t_rest > REC_MAX_REST_MS)
            {
                u4t_rest = REC_MAX_REST_MS;
            }
            g_sequence[g_u2t_seq_count - 1U].rest_ms = (uint16_t)u4t_rest;
        }
        else
        {
            /* First step of the sequence */
        }
        g_s1t_rec_note = s1t_note;
        g_u4t_rec_note_start_ms = u4t_now;
    }
    else if (s1t_note < 0)
    {
        if ((u4t_now - g_u4t_blink_ms) >= REC_LED_BLINK_MS)
        {
            g_u4t_blink_ms = u4t_now;
            g_b_blink_on = (g_b_blink_on == false);
            bsp_gpio_led_red_set(g_b_blink_on);
        }
        if (g_s1t_rec_note >= 0)
        {
            synth_rec_close_note(u4t_now, REC_DEF_REST_MS);
            g_u4t_rec_last_release_ms = u4t_now;
        }
    }
    else
    {
        /* Same note still held */
    }

    if (g_u2t_seq_count >= SYNTH_MAX_STEPS)
    {
        synth_set_mode(MODE_LIVE, u4t_now);
    }
}

/* Playback Mode: note phase then rest phase per step, looping back to step 0 forever */
static void synth_update_playback(uint32_t u4t_now, uint8_t u1t_vol_pct)
{
    const synth_step_t *p_step = &g_sequence[g_u2t_play_step];
    uint32_t u4t_elapsed = u4t_now - g_u4t_play_step_start_ms;

    if (g_b_play_in_note == true)
    {
        synth_sound(NOTE_FREQ[p_step->note_index], u1t_vol_pct);
        if (u4t_elapsed >= (uint32_t)p_step->duration_ms)
        {
            g_b_play_in_note = false;
            g_u4t_play_step_start_ms = u4t_now;
            bsp_buzzer_off();
            bsp_gpio_led_red_set(false);
        }
    }
    else if ((u4t_elapsed >= (uint32_t)p_step->rest_ms) && (u4t_elapsed >= PLAY_MIN_GAP_MS))
    {
        g_u2t_play_step = (uint16_t)((g_u2t_play_step + 1U) % g_u2t_seq_count);
        g_b_play_in_note = true;
        g_u4t_play_step_start_ms = u4t_now;
        if (g_u2t_play_step == 0U)
        {
            bsp_uart_send_string("[PLAYBACK] Loop restart...\r\n");
        }
        p_step = &g_sequence[g_u2t_play_step];
        synth_log_note("[PLAYBACK] Playing: ", p_step->note_index, (uint32_t)p_step->duration_ms);
    }
    else
    {
        /* Resting between steps */
    }
}

/* Live / Recording Sound: modulated note while held, silence otherwise */
static void synth_update_live_sound(int8_t s1t_note, uint8_t u1t_vol_pct, int32_t s4t_norm_x, int32_t s4t_norm_y,
                                    uint32_t u4t_now)
{
    if (s1t_note >= 0)
    {
        if (s1t_note != g_s1t_last_played)
        {
            g_u4t_lfo_start_ms = u4t_now;    /* Restart vibrato phase on each new note */
            g_s1t_last_played = s1t_note;
            synth_log_note("[KEY] Playing: ", s1t_note, 0U);
        }
        synth_sound(synth_modulated_freq(s1t_note, s4t_norm_x, s4t_norm_y, u4t_now), u1t_vol_pct);
    }
    else
    {
        bsp_buzzer_off();
        if (g_mode != MODE_RECORDING)
        {
            bsp_gpio_led_red_set(false);    /* While recording the LED blinks instead */
        }
        g_s1t_last_played = -1;
    }
}

/* OLED Virtual Piano UI: redraw framebuffer every 30 ms; bsp_oled_service streams it via DMA */
static void synth_update_display(uint32_t u4t_now, int8_t s1t_note, uint8_t u1t_vol_pct,
                                 int32_t s4t_norm_x, int32_t s4t_norm_y)
{
    static uint32_t u4t_last_render_ms = 0U;
    const char *p_mode = "[LIVE]";
    const char *p_name = "-- SILENT --";
    const char *p_freq = "";
    int8_t s1t_key = s1t_note;

    if (g_mode == MODE_RECORDING)
    {
        p_mode = "[   ]";
        if (g_b_blink_on == true)
        {
            p_mode = "[REC]";
        }
    }
    else if (g_mode == MODE_PLAYING)
    {
        p_mode = "[PLAY]";
        s1t_key = -1;
        if (g_b_play_in_note == true)
        {
            s1t_key = g_sequence[g_u2t_play_step].note_index;
        }
    }
    else
    {
        /* Live mode shows the active note */
    }
    if (s1t_key >= 0)
    {
        p_name = NOTE_NAMES[s1t_key];
        p_freq = NOTE_FREQS[s1t_key];
    }

    if ((u4t_now - u4t_last_render_ms) >= OLED_RENDER_INTERVAL_MS)
    {
        u4t_last_render_ms = u4t_now;
        bsp_oled_clear_buffer();
        bsp_oled_render_header(p_mode, (s4t_norm_y <= JOY_HIGH_BANK_THRESHOLD), u1t_vol_pct, p_name, p_freq,
                               (s4t_norm_x * BEND_MAX_CENTS) / JOY_NORM_FULL);
        bsp_oled_render_pitch_gauge(s4t_norm_x);
        bsp_oled_render_piano_keyboard(s1t_key, KEY_LABELS);
    }
    bsp_oled_service(u4t_now);
}

/* Serial Commands: '1'..'8' note, 'r' record, 'p' play, 'c' clear, '?' help */
static void synth_handle_uart_rx(uint32_t u4t_now)
{
    char c_cmd = '\0';

    while (bsp_uart_read_char(&c_cmd) == true)
    {
        if ((c_cmd >= '1') && (c_cmd <= '8'))
        {
            g_s1t_uart_note = (int8_t)(c_cmd - '1');
            g_u4t_uart_note_ms = u4t_now;
            bsp_uart_send_string("[CMD] Trigger: ");
            bsp_uart_send_string(NOTE_NAMES[g_s1t_uart_note]);
            bsp_uart_send_string("\r\n");
        }
        else if ((c_cmd == 'r') || (c_cmd == 'R'))
        {
            synth_toggle_mode(MODE_RECORDING, u4t_now);
        }
        else if ((c_cmd == 'p') || (c_cmd == 'P'))
        {
            synth_toggle_mode(MODE_PLAYING, u4t_now);
        }
        else if ((c_cmd == 'c') || (c_cmd == 'C'))
        {
            synth_set_mode(MODE_LIVE, u4t_now);
            g_u2t_seq_count = 0U;
            bsp_uart_send_string("[RECORDER] Memory cleared!\r\n");
        }
        else if (c_cmd == '?')
        {
            bsp_uart_send_string("\r\n=== STM32 Synthesizer Serial Commands ===\r\n");
            bsp_uart_send_string("  '1'..'8' : Play Note (Do..C8)\r\n");
            bsp_uart_send_string("  'r'/'R'  : Toggle Recording\r\n");
            bsp_uart_send_string("  'p'/'P'  : Toggle Playback\r\n");
            bsp_uart_send_string("  'c'/'C'  : Clear Sequence Memory\r\n");
            bsp_uart_send_string("  '?'      : Show Help Menu\r\n\r\n");
        }
        else
        {
            /* Ignore unrecognized characters */
        }
    }
}

/* Public Application Lifecycle */
void app_synth_init(void)
{
    bsp_uart_send_string("\r\n=== STM32 Synthesizer (C7-C8 + OLED) | 115200 bps | '?' for Help ===\r\n");
    for (uint8_t u1t_i = 0U; u1t_i < CHIME_NUM_NOTES; u1t_i++)
    {
        bsp_buzzer_set_tone(NOTE_FREQ[u1t_i * 2U], TONE_VOL_RAW);    /* DO-MI-SOL startup chime */
        bsp_delay_ms(TONE_STEP_MS);
    }
    bsp_buzzer_off();
}

void app_synth_run(void)
{
    while (true)
    {
        uint32_t u4t_now = bsp_timer_get_ms();
        joy_sw_event_t joy_evt = JOY_SW_EVT_NONE;

        if (bsp_gpio_get_exti_flag() == true)
        {
            bsp_gpio_clear_exti_flag();    /* Key 1 EXTI event; the key itself is read by the debouncer */
        }
        bsp_joystick_service(u4t_now);
        synth_handle_uart_rx(u4t_now);

        /* Joystick SW: short press ends recording / toggles playback, long press toggles recording */
        joy_evt = bsp_joystick_get_event();
        if ((joy_evt == JOY_SW_EVT_LONG_PRESS) || ((joy_evt == JOY_SW_EVT_SHORT_PRESS) && (g_mode == MODE_RECORDING)))
        {
            synth_toggle_mode(MODE_RECORDING, u4t_now);
        }
        else if (joy_evt == JOY_SW_EVT_SHORT_PRESS)
        {
            synth_toggle_mode(MODE_PLAYING, u4t_now);
        }
        else
        {
            /* No switch event */
        }

        /* Sample inputs once per superloop pass, then update sound, recorder and display */
        int32_t s4t_norm_x = bsp_joystick_get_norm_x();
        int32_t s4t_norm_y = bsp_joystick_get_norm_y();
        uint8_t u1t_vol_pct = bsp_adc_get_volume_percent();
        int8_t s1t_note = synth_read_active_note(synth_scan_keys(u4t_now), s4t_norm_y, u4t_now);

        if (g_mode == MODE_PLAYING)
        {
            synth_update_playback(u4t_now, u1t_vol_pct);
        }
        else
        {
            if (g_mode == MODE_RECORDING)
            {
                synth_update_recording(u4t_now, s1t_note);
            }
            synth_update_live_sound(s1t_note, u1t_vol_pct, s4t_norm_x, s4t_norm_y, u4t_now);
        }
        synth_update_display(u4t_now, s1t_note, u1t_vol_pct, s4t_norm_x, s4t_norm_y);
    }
}
