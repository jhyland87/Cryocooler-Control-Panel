#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DASHBOARD_VALUE_BUF_SIZE 32

typedef enum {
    CTRL_ACTION_START,
    CTRL_ACTION_STOP,
    CTRL_ACTION_REINIT,
    CTRL_ACTION_OFF,
    CTRL_ACTION_FAULT_CLEAR
} dashboard_ctrl_action_t;

typedef void (*dashboard_ctrl_cb_t)(dashboard_ctrl_action_t action);

typedef struct {
    char runtime_since_powerup[DASHBOARD_VALUE_BUF_SIZE];
    char time_cryo_running[DASHBOARD_VALUE_BUF_SIZE];
    char cryo_accumulated_runtime[DASHBOARD_VALUE_BUF_SIZE];
    char system_accumulated_runtime[DASHBOARD_VALUE_BUF_SIZE];

    bool status_led;
    bool bypass_led;

    bool bypass_relay;
    bool force_bypass_relay;

    bool power_current_loop_regulating;
    bool power_loop_active;
    bool power_duty_limit_exceeded;
    bool power_drive_limit_foldback;
    bool auto_frequency_adjust;
    bool temperature_loop_regulating;
    bool cold_stage_narrow_temp_oob;
    bool cold_stage_wide_temp_oob;
    bool cooler_rejection_temp_oob;
    bool ambient_temp_oob;

    float cold_stage_narrow_temp;
    float cold_stage_wide_temp;
    float cooler_rejection_temp;
    float ambient_temp;
    float input_voltage;
    float cooler_power;
    float cooler_power_error;
    float cooler_voltage_real;
    float cooler_current_real;
    float cooler_current_phase;
    float cooler_impedance;
    float cooler_current_rms;
    float cooler_frequency;
    float cooler_power_drive;

    char state_machine_mode[DASHBOARD_VALUE_BUF_SIZE];
    char state_machine_state[DASHBOARD_VALUE_BUF_SIZE];
} dashboard_data_t;

void dashboard_init(void);
void dashboard_tick(void);
dashboard_data_t *dashboard_get_data(void);
void dashboard_mark_dirty(void);

typedef enum {
    CH_CHART_TEMP_K,
    CH_CHART_COOLING_RATE,
    CH_CHART_COOLDOWN_PCT,
    CH_CHART_DELTA_AMBIENT,
    CH_CHART_VOLTAGE,
    CH_CHART_CURRENT,
    CH_CHART_COUNT
} cold_head_chart_t;

void dashboard_add_cold_head_reading(cold_head_chart_t chart, float value);
void dashboard_set_ctrl_callback(dashboard_ctrl_cb_t cb);
void dashboard_set_espnow_message(const char *msg);
void dashboard_set_espnow_status(bool connected);

/**
 * Display a command response string in the dedicated response area on the
 * Control tab.  Called from the LVGL task after an ESP-NOW response packet
 * arrives from the cryocooler.  @p msg must be a null-terminated C string.
 */
void dashboard_set_cmd_response(const char *msg);

/**
 * Append a line of text to the Console tab.
 * Called from ui_task after draining the console_log ring buffer.
 * Must be called with the LVGL API lock held.
 */
void dashboard_console_append(const char *line);

/**
 * Clear all text currently displayed in the Console tab.
 */
void dashboard_console_clear(void);

/**
 * Callback type invoked when the user submits a command via the on-screen
 * keyboard on the Console tab.  @p cmd is a null-terminated command string
 * (no trailing newline).
 */
typedef void (*dashboard_send_cmd_cb_t)(const char *cmd);

/**
 * Register the callback used to transmit keyboard-entered commands to the
 * cryocooler (typically via ESP-NOW).  Must be called before the first
 * use of the Console tab's keyboard.
 */
void dashboard_set_send_cmd_callback(dashboard_send_cmd_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* DASHBOARD_H */
