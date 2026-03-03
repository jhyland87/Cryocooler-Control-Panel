#include "dashboard.h"
#include <stdio.h>
#include <string.h>

static dashboard_data_t s_data;
static bool s_dirty = true;

static lv_obj_t *s_time_table;
static lv_obj_t *s_led_table;
static lv_obj_t *s_relay_table;
static lv_obj_t *s_system_table;
static lv_obj_t *s_measure_table;
static lv_obj_t *s_state_table;

#define CHART_POINT_COUNT  60
#define CHART_Y_MIN        200
#define CHART_Y_MAX        500
#define CHART_X_LABEL_COUNT 7
#define CHART_Y_LABEL_COUNT 7

static dashboard_ctrl_cb_t s_ctrl_cb;

static lv_obj_t *s_temp_chart;
static lv_chart_series_t *s_temp_series;
static lv_obj_t *s_chart_x_labels[CHART_X_LABEL_COUNT];
static lv_obj_t *s_chart_y_labels[CHART_Y_LABEL_COUNT];
static uint16_t s_chart_start_minutes;

static const char *bool_to_on_off(bool val) {
    return val ? "On" : "Off";
}

static const char *bool_to_true_false(bool val) {
    return val ? "True" : "False";
}

static void init_stub_data(void) {
    snprintf(s_data.runtime_since_powerup, DASHBOARD_VALUE_BUF_SIZE, "00:12:34");
    snprintf(s_data.time_cryo_running, DASHBOARD_VALUE_BUF_SIZE, "00:10:00");
    snprintf(s_data.cryo_accumulated_runtime, DASHBOARD_VALUE_BUF_SIZE, "1234:56:78");
    snprintf(s_data.system_accumulated_runtime, DASHBOARD_VALUE_BUF_SIZE, "5678:90:12");

    s_data.status_led = false;
    s_data.bypass_led = false;

    s_data.bypass_relay = false;
    s_data.force_bypass_relay = false;

    s_data.power_current_loop_regulating = false;
    s_data.power_loop_active = false;
    s_data.power_duty_limit_exceeded = false;
    s_data.power_drive_limit_foldback = false;
    s_data.auto_frequency_adjust = true;
    s_data.temperature_loop_regulating = false;
    s_data.cold_stage_narrow_temp_oob = false;
    s_data.cold_stage_wide_temp_oob = false;
    s_data.cooler_rejection_temp_oob = false;
    s_data.ambient_temp_oob = false;

    s_data.cold_stage_narrow_temp = 123.4f;
    s_data.cold_stage_wide_temp = 123.4f;
    s_data.cooler_rejection_temp = 32.5f;
    s_data.ambient_temp = 23.4f;
    s_data.input_voltage = 12.4f;
    s_data.cooler_power = 123.4f;
    s_data.cooler_power_error = 0.0f;
    s_data.cooler_voltage_real = 9.0f;
    s_data.cooler_current_real = 4.6f;
    s_data.cooler_current_phase = 30.3f;
    s_data.cooler_impedance = 1.8f;
    s_data.cooler_current_rms = 4.6f;
    s_data.cooler_frequency = 60.0f;
    s_data.cooler_power_drive = 79.0f;

    snprintf(s_data.state_machine_mode, DASHBOARD_VALUE_BUF_SIZE, "Automatic");
    snprintf(s_data.state_machine_state, DASHBOARD_VALUE_BUF_SIZE, "Initialize");
}

/* Header row draw customization: dark background with white text */
static void table_draw_event_cb(lv_event_t *e) {
    lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t *base_dsc = (lv_draw_dsc_base_t *)lv_draw_task_get_draw_dsc(draw_task);

    if (base_dsc->part != LV_PART_ITEMS) return;

    uint32_t row = base_dsc->id1;
    if (row != 0) return;

    lv_draw_task_type_t type = lv_draw_task_get_type(draw_task);

    if (type == LV_DRAW_TASK_TYPE_FILL) {
        lv_draw_fill_dsc_t *fill_dsc = (lv_draw_fill_dsc_t *)base_dsc;
        fill_dsc->color = lv_color_hex(0x003366);
        fill_dsc->opa = LV_OPA_COVER;
    }
    else if (type == LV_DRAW_TASK_TYPE_LABEL) {
        lv_draw_label_dsc_t *label_dsc = (lv_draw_label_dsc_t *)base_dsc;
        label_dsc->color = lv_color_hex(0xFFFFFF);
    }
}

static void apply_table_theme(lv_obj_t *table) {
    lv_obj_set_style_border_width(table, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(table, lv_color_hex(0xCCCCCC), LV_PART_ITEMS);
    lv_obj_set_style_border_side(table, LV_BORDER_SIDE_FULL, LV_PART_ITEMS);
    lv_obj_set_style_pad_top(table, 8, LV_PART_ITEMS);
    lv_obj_set_style_pad_bottom(table, 8, LV_PART_ITEMS);
    lv_obj_set_style_pad_left(table, 10, LV_PART_ITEMS);
    lv_obj_set_style_pad_right(table, 10, LV_PART_ITEMS);

    lv_obj_add_event_cb(table, table_draw_event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);
    lv_obj_add_flag(table, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
}

/* ── Control Tab ─────────────────────────────────────── */

static void ctrl_btn_event_cb(lv_event_t *e) {
    dashboard_ctrl_action_t action = (dashboard_ctrl_action_t)(intptr_t)lv_event_get_user_data(e);
    if (s_ctrl_cb) {
        s_ctrl_cb(action);
    }
}

static lv_obj_t *create_ctrl_button(lv_obj_t *parent, const char *label_text,
                                     lv_color_t bg_color, dashboard_ctrl_action_t action) {
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 320, 70);
    lv_obj_set_style_bg_color(btn, bg_color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_shadow_offset_y(btn, 3, LV_PART_MAIN);

    lv_obj_set_style_bg_color(btn, lv_color_darken(bg_color, LV_OPA_20),
                              (lv_style_selector_t)(LV_PART_MAIN | LV_STATE_PRESSED));

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(lbl);

    lv_obj_add_event_cb(btn, ctrl_btn_event_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)action);
    return btn;
}

static void create_control_tab(lv_obj_t *parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 30, LV_PART_MAIN);
    lv_obj_set_style_pad_row(parent, 30, LV_PART_MAIN);
    lv_obj_set_style_pad_column(parent, 40, LV_PART_MAIN);

    create_ctrl_button(parent, "Start Cryocooler", lv_color_hex(0x2E7D32), CTRL_ACTION_START_CRYO);
    create_ctrl_button(parent, "Stop Cryocooler",  lv_color_hex(0xC62828), CTRL_ACTION_STOP_CRYO);
    create_ctrl_button(parent, "Shutdown",          lv_color_hex(0xE65100), CTRL_ACTION_SHUTDOWN);
    create_ctrl_button(parent, "Reset",             lv_color_hex(0x1565C0), CTRL_ACTION_RESET);
}

/* ── Time Tab ────────────────────────────────────────── */

static void create_time_tab(lv_obj_t *parent) {
    s_time_table = lv_table_create(parent);
    lv_table_set_column_count(s_time_table, 2);
    lv_table_set_row_count(s_time_table, 5);
    lv_table_set_column_width(s_time_table, 0, 500);
    lv_table_set_column_width(s_time_table, 1, 260);

    lv_table_set_cell_value(s_time_table, 0, 0, "Name");
    lv_table_set_cell_value(s_time_table, 0, 1, "Value");

    lv_table_set_cell_value(s_time_table, 1, 0, "Runtime since powerup");
    lv_table_set_cell_value(s_time_table, 2, 0, "Time Cryocooler has been running");
    lv_table_set_cell_value(s_time_table, 3, 0, "Cryocooler accumulated run time");
    lv_table_set_cell_value(s_time_table, 4, 0, "System accumulated run time");

    apply_table_theme(s_time_table);
}

static void refresh_time_tab(void) {
    lv_table_set_cell_value(s_time_table, 1, 1, s_data.runtime_since_powerup);
    lv_table_set_cell_value(s_time_table, 2, 1, s_data.time_cryo_running);
    lv_table_set_cell_value(s_time_table, 3, 1, s_data.cryo_accumulated_runtime);
    lv_table_set_cell_value(s_time_table, 4, 1, s_data.system_accumulated_runtime);
}

/* ── LED Tab ─────────────────────────────────────────── */

static void create_led_tab(lv_obj_t *parent) {
    s_led_table = lv_table_create(parent);
    lv_table_set_column_count(s_led_table, 2);
    lv_table_set_row_count(s_led_table, 3);
    lv_table_set_column_width(s_led_table, 0, 500);
    lv_table_set_column_width(s_led_table, 1, 260);

    lv_table_set_cell_value(s_led_table, 0, 0, "Name");
    lv_table_set_cell_value(s_led_table, 0, 1, "Status");

    lv_table_set_cell_value(s_led_table, 1, 0, "Status LED");
    lv_table_set_cell_value(s_led_table, 2, 0, "Bypass LED");

    apply_table_theme(s_led_table);
}

static void refresh_led_tab(void) {
    lv_table_set_cell_value(s_led_table, 1, 1, bool_to_on_off(s_data.status_led));
    lv_table_set_cell_value(s_led_table, 2, 1, bool_to_on_off(s_data.bypass_led));
}

/* ── Relay Tab ───────────────────────────────────────── */

static void create_relay_tab(lv_obj_t *parent) {
    s_relay_table = lv_table_create(parent);
    lv_table_set_column_count(s_relay_table, 2);
    lv_table_set_row_count(s_relay_table, 3);
    lv_table_set_column_width(s_relay_table, 0, 500);
    lv_table_set_column_width(s_relay_table, 1, 260);

    lv_table_set_cell_value(s_relay_table, 0, 0, "Name");
    lv_table_set_cell_value(s_relay_table, 0, 1, "Status");

    lv_table_set_cell_value(s_relay_table, 1, 0, "Bypass Relay");
    lv_table_set_cell_value(s_relay_table, 2, 0, "Force Bypass Relay");

    apply_table_theme(s_relay_table);
}

static void refresh_relay_tab(void) {
    lv_table_set_cell_value(s_relay_table, 1, 1, bool_to_on_off(s_data.bypass_relay));
    lv_table_set_cell_value(s_relay_table, 2, 1, bool_to_on_off(s_data.force_bypass_relay));
}

/* ── System Tab ──────────────────────────────────────── */

static void create_system_tab(lv_obj_t *parent) {
    s_system_table = lv_table_create(parent);
    lv_table_set_column_count(s_system_table, 2);
    lv_table_set_row_count(s_system_table, 11);
    lv_table_set_column_width(s_system_table, 0, 560);
    lv_table_set_column_width(s_system_table, 1, 200);

    lv_table_set_cell_value(s_system_table, 0, 0, "Name");
    lv_table_set_cell_value(s_system_table, 0, 1, "Status");

    lv_table_set_cell_value(s_system_table, 1, 0, "Power/Current loop regulating");
    lv_table_set_cell_value(s_system_table, 2, 0, "Power loop active");
    lv_table_set_cell_value(s_system_table, 3, 0, "Power duty limit exceeded");
    lv_table_set_cell_value(s_system_table, 4, 0, "Power drive limit foldback");
    lv_table_set_cell_value(s_system_table, 5, 0, "Auto frequency adjust");
    lv_table_set_cell_value(s_system_table, 6, 0, "Temperature loop regulating");
    lv_table_set_cell_value(s_system_table, 7, 0, "Cold stage narrow range temp OOB");
    lv_table_set_cell_value(s_system_table, 8, 0, "Cold stage wide range temp OOB");
    lv_table_set_cell_value(s_system_table, 9, 0, "Cooler rejection temp OOB");
    lv_table_set_cell_value(s_system_table, 10, 0, "Ambient temperature OOB");

    apply_table_theme(s_system_table);
}

static void refresh_system_tab(void) {
    lv_table_set_cell_value(s_system_table, 1, 1, bool_to_true_false(s_data.power_current_loop_regulating));
    lv_table_set_cell_value(s_system_table, 2, 1, bool_to_true_false(s_data.power_loop_active));
    lv_table_set_cell_value(s_system_table, 3, 1, bool_to_true_false(s_data.power_duty_limit_exceeded));
    lv_table_set_cell_value(s_system_table, 4, 1, bool_to_true_false(s_data.power_drive_limit_foldback));
    lv_table_set_cell_value(s_system_table, 5, 1, bool_to_true_false(s_data.auto_frequency_adjust));
    lv_table_set_cell_value(s_system_table, 6, 1, bool_to_true_false(s_data.temperature_loop_regulating));
    lv_table_set_cell_value(s_system_table, 7, 1, bool_to_true_false(s_data.cold_stage_narrow_temp_oob));
    lv_table_set_cell_value(s_system_table, 8, 1, bool_to_true_false(s_data.cold_stage_wide_temp_oob));
    lv_table_set_cell_value(s_system_table, 9, 1, bool_to_true_false(s_data.cooler_rejection_temp_oob));
    lv_table_set_cell_value(s_system_table, 10, 1, bool_to_true_false(s_data.ambient_temp_oob));
}

/* ── Measure Tab ─────────────────────────────────────── */

static void create_measure_tab(lv_obj_t *parent) {
    s_measure_table = lv_table_create(parent);
    lv_table_set_column_count(s_measure_table, 3);
    lv_table_set_row_count(s_measure_table, 15);
    lv_table_set_column_width(s_measure_table, 0, 440);
    lv_table_set_column_width(s_measure_table, 1, 180);
    lv_table_set_column_width(s_measure_table, 2, 140);

    lv_table_set_cell_value(s_measure_table, 0, 0, "Name");
    lv_table_set_cell_value(s_measure_table, 0, 1, "Value");
    lv_table_set_cell_value(s_measure_table, 0, 2, "Unit");

    lv_table_set_cell_value(s_measure_table, 1, 0, "Cold stage narrow range temperature");
    lv_table_set_cell_value(s_measure_table, 2, 0, "Cold stage wide range temperature");
    lv_table_set_cell_value(s_measure_table, 3, 0, "Cooler rejection temperature");
    lv_table_set_cell_value(s_measure_table, 4, 0, "Ambient temperature");
    lv_table_set_cell_value(s_measure_table, 5, 0, "Input voltage");
    lv_table_set_cell_value(s_measure_table, 6, 0, "Cooler power");
    lv_table_set_cell_value(s_measure_table, 7, 0, "Cooler power error");
    lv_table_set_cell_value(s_measure_table, 8, 0, "Cooler voltage real");
    lv_table_set_cell_value(s_measure_table, 9, 0, "Cooler current real");
    lv_table_set_cell_value(s_measure_table, 10, 0, "Cooler current phase");
    lv_table_set_cell_value(s_measure_table, 11, 0, "Cooler impedance");
    lv_table_set_cell_value(s_measure_table, 12, 0, "Cooler current RMS");
    lv_table_set_cell_value(s_measure_table, 13, 0, "Cooler frequency");
    lv_table_set_cell_value(s_measure_table, 14, 0, "Cooler power drive");

    lv_table_set_cell_value(s_measure_table, 1, 2, "K");
    lv_table_set_cell_value(s_measure_table, 2, 2, "K");
    lv_table_set_cell_value(s_measure_table, 3, 2, "C");
    lv_table_set_cell_value(s_measure_table, 4, 2, "C");
    lv_table_set_cell_value(s_measure_table, 5, 2, "Vdc");
    lv_table_set_cell_value(s_measure_table, 6, 2, "W");
    lv_table_set_cell_value(s_measure_table, 7, 2, "W");
    lv_table_set_cell_value(s_measure_table, 8, 2, "Vrms");
    lv_table_set_cell_value(s_measure_table, 9, 2, "Arms");
    lv_table_set_cell_value(s_measure_table, 10, 2, "\xC2\xB0");
    lv_table_set_cell_value(s_measure_table, 11, 2, "ohm");
    lv_table_set_cell_value(s_measure_table, 12, 2, "Arms");
    lv_table_set_cell_value(s_measure_table, 13, 2, "Hz");
    lv_table_set_cell_value(s_measure_table, 14, 2, "%");

    apply_table_theme(s_measure_table);
}

static void refresh_measure_tab(void) {
    lv_table_set_cell_value_fmt(s_measure_table, 1, 1, "%.1f", (double)s_data.cold_stage_narrow_temp);
    lv_table_set_cell_value_fmt(s_measure_table, 2, 1, "%.1f", (double)s_data.cold_stage_wide_temp);
    lv_table_set_cell_value_fmt(s_measure_table, 3, 1, "%.1f", (double)s_data.cooler_rejection_temp);
    lv_table_set_cell_value_fmt(s_measure_table, 4, 1, "%.1f", (double)s_data.ambient_temp);
    lv_table_set_cell_value_fmt(s_measure_table, 5, 1, "%.1f", (double)s_data.input_voltage);
    lv_table_set_cell_value_fmt(s_measure_table, 6, 1, "%.1f", (double)s_data.cooler_power);
    lv_table_set_cell_value_fmt(s_measure_table, 7, 1, "%.1f", (double)s_data.cooler_power_error);
    lv_table_set_cell_value_fmt(s_measure_table, 8, 1, "%.1f", (double)s_data.cooler_voltage_real);
    lv_table_set_cell_value_fmt(s_measure_table, 9, 1, "%.1f", (double)s_data.cooler_current_real);
    lv_table_set_cell_value_fmt(s_measure_table, 10, 1, "%.1f", (double)s_data.cooler_current_phase);
    lv_table_set_cell_value_fmt(s_measure_table, 11, 1, "%.1f", (double)s_data.cooler_impedance);
    lv_table_set_cell_value_fmt(s_measure_table, 12, 1, "%.1f", (double)s_data.cooler_current_rms);
    lv_table_set_cell_value_fmt(s_measure_table, 13, 1, "%.1f", (double)s_data.cooler_frequency);
    lv_table_set_cell_value_fmt(s_measure_table, 14, 1, "%.1f", (double)s_data.cooler_power_drive);
}

/* ── State Tab ───────────────────────────────────────── */

static void create_state_tab(lv_obj_t *parent) {
    s_state_table = lv_table_create(parent);
    lv_table_set_column_count(s_state_table, 2);
    lv_table_set_row_count(s_state_table, 3);
    lv_table_set_column_width(s_state_table, 0, 500);
    lv_table_set_column_width(s_state_table, 1, 260);

    lv_table_set_cell_value(s_state_table, 0, 0, "Name");
    lv_table_set_cell_value(s_state_table, 0, 1, "Value");

    lv_table_set_cell_value(s_state_table, 1, 0, "State Machine Mode");
    lv_table_set_cell_value(s_state_table, 2, 0, "State Machine State");

    apply_table_theme(s_state_table);
}

static void refresh_state_tab(void) {
    lv_table_set_cell_value(s_state_table, 1, 1, s_data.state_machine_mode);
    lv_table_set_cell_value(s_state_table, 2, 1, s_data.state_machine_state);
}

/* ── Chart Tab ───────────────────────────────────────── */

static void update_chart_x_labels(void) {
    const int offsets[] = {0, 10, 20, 30, 40, 50, 59};
    for (int i = 0; i < CHART_X_LABEL_COUNT; i++) {
        uint16_t minutes = (s_chart_start_minutes + offsets[i]) % (24 * 60);
        char buf[8];
        snprintf(buf, sizeof(buf), "%02u:%02u", minutes / 60, minutes % 60);
        lv_label_set_text(s_chart_x_labels[i], buf);
    }
}

static void init_chart_stub_data(void) {
    s_chart_start_minutes = 8 * 60;

    for (int i = 0; i < CHART_POINT_COUNT; i++) {
        float base = 30.0f + (5.0f * i / CHART_POINT_COUNT);
        float noise = ((float)((i * 7 + 13) % 20) - 10.0f) * 0.15f;
        int32_t val = (int32_t)((base + noise) * 10.0f);
        lv_chart_set_next_value(s_temp_chart, s_temp_series, val);
    }

    update_chart_x_labels();
}

static void create_chart_tab(lv_obj_t *parent) {
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Cooler Rejection Temperature");
    lv_obj_set_style_text_color(title, lv_color_hex(0x003366), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 20, 4);

    lv_obj_t *unit = lv_label_create(parent);
    lv_label_set_text(unit, "\xC2\xB0""C");
    lv_obj_set_style_text_color(unit, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_pos(unit, 50, 18);

    s_temp_chart = lv_chart_create(parent);
    lv_obj_set_size(s_temp_chart, 680, 340);
    lv_obj_set_pos(s_temp_chart, 75, 30);
    lv_chart_set_type(s_temp_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_temp_chart, CHART_POINT_COUNT);
    lv_chart_set_axis_range(s_temp_chart, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN, CHART_Y_MAX);
    lv_chart_set_div_line_count(s_temp_chart, 5, 5);
    lv_chart_set_update_mode(s_temp_chart, LV_CHART_UPDATE_MODE_SHIFT);

    lv_obj_set_style_size(s_temp_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(s_temp_chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(s_temp_chart, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_temp_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_temp_chart, lv_color_hex(0xBBBBBB), LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_temp_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_temp_chart, lv_color_hex(0xFAFAFA), LV_PART_MAIN);

    s_temp_series = lv_chart_add_series(s_temp_chart, lv_color_hex(0xFF6600), LV_CHART_AXIS_PRIMARY_Y);

    int y_vals[] = {50, 45, 40, 35, 30, 25, 20};
    for (int i = 0; i < CHART_Y_LABEL_COUNT; i++) {
        s_chart_y_labels[i] = lv_label_create(parent);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", y_vals[i]);
        lv_label_set_text(s_chart_y_labels[i], buf);
        lv_obj_set_style_text_color(s_chart_y_labels[i], lv_color_hex(0x666666), LV_PART_MAIN);
        int y = 30 + (340 * i / (CHART_Y_LABEL_COUNT - 1)) - 7;
        lv_obj_set_pos(s_chart_y_labels[i], 45, y);
    }

    const int x_offsets[] = {0, 10, 20, 30, 40, 50, 59};
    for (int i = 0; i < CHART_X_LABEL_COUNT; i++) {
        s_chart_x_labels[i] = lv_label_create(parent);
        lv_label_set_text(s_chart_x_labels[i], "--:--");
        lv_obj_set_style_text_color(s_chart_x_labels[i], lv_color_hex(0x666666), LV_PART_MAIN);
        int x = 75 + (680 * x_offsets[i] / (CHART_POINT_COUNT - 1)) - 15;
        lv_obj_set_pos(s_chart_x_labels[i], x, 375);
    }

    init_chart_stub_data();
}

/* ── Public API ──────────────────────────────────────── */

void dashboard_init(void) {
    init_stub_data();

    lv_obj_t *tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tv, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tv, 40);
    lv_obj_set_size(tv, 800, 480);

    lv_obj_set_style_bg_color(lv_tabview_get_tab_bar(tv), lv_color_hex(0x003366), LV_PART_MAIN);
    lv_obj_set_style_text_color(lv_tabview_get_tab_bar(tv), lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_side(lv_tabview_get_tab_bar(tv), LV_BORDER_SIDE_BOTTOM, LV_PART_ITEMS);
    lv_obj_set_style_border_width(lv_tabview_get_tab_bar(tv), 3,
                                 (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_CHECKED));
    lv_obj_set_style_border_color(lv_tabview_get_tab_bar(tv), lv_color_hex(0x66BBFF),
                                  (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_CHECKED));

    lv_obj_t *tab_ctrl    = lv_tabview_add_tab(tv, "Control");
    lv_obj_t *tab_time    = lv_tabview_add_tab(tv, "Time");
    lv_obj_t *tab_led     = lv_tabview_add_tab(tv, "LED");
    lv_obj_t *tab_relay   = lv_tabview_add_tab(tv, "Relay");
    lv_obj_t *tab_system  = lv_tabview_add_tab(tv, "System");
    lv_obj_t *tab_measure = lv_tabview_add_tab(tv, "Measure");
    lv_obj_t *tab_state   = lv_tabview_add_tab(tv, "State");
    lv_obj_t *tab_chart   = lv_tabview_add_tab(tv, "Chart");

    create_control_tab(tab_ctrl);
    create_time_tab(tab_time);
    create_led_tab(tab_led);
    create_relay_tab(tab_relay);
    create_system_tab(tab_system);
    create_measure_tab(tab_measure);
    create_state_tab(tab_state);
    create_chart_tab(tab_chart);

    s_dirty = true;
    dashboard_tick();
}

void dashboard_tick(void) {
    if (!s_dirty) return;
    s_dirty = false;

    refresh_time_tab();
    refresh_led_tab();
    refresh_relay_tab();
    refresh_system_tab();
    refresh_measure_tab();
    refresh_state_tab();
}

dashboard_data_t *dashboard_get_data(void) {
    return &s_data;
}

void dashboard_mark_dirty(void) {
    s_dirty = true;
}

void dashboard_set_ctrl_callback(dashboard_ctrl_cb_t cb) {
    s_ctrl_cb = cb;
}

void dashboard_add_temp_reading(float temp_c, uint8_t hour, uint8_t minute) {
    int32_t val = (int32_t)(temp_c * 10.0f);
    lv_chart_set_next_value(s_temp_chart, s_temp_series, val);

    uint16_t new_end = (uint16_t)(hour) * 60 + minute;
    if (new_end >= (CHART_POINT_COUNT - 1)) {
        s_chart_start_minutes = new_end - (CHART_POINT_COUNT - 1);
    } else {
        s_chart_start_minutes = (24 * 60) + new_end - (CHART_POINT_COUNT - 1);
    }

    update_chart_x_labels();
}
