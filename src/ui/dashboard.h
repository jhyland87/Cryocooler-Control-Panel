#ifndef DASHBOARD_H
#define DASHBOARD_H

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DASHBOARD_VALUE_BUF_SIZE 32

typedef enum {
    CTRL_ACTION_START_CRYO,
    CTRL_ACTION_STOP_CRYO,
    CTRL_ACTION_SHUTDOWN,
    CTRL_ACTION_RESET
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

void dashboard_add_temp_reading(float temp_c, uint8_t hour, uint8_t minute);
void dashboard_set_ctrl_callback(dashboard_ctrl_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* DASHBOARD_H */
