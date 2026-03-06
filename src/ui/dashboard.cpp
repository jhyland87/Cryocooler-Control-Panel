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

#define CH_POINT_COUNT 60

static dashboard_ctrl_cb_t s_ctrl_cb;

static lv_obj_t          *s_ch_charts[CH_CHART_COUNT];
static lv_chart_series_t *s_ch_series[CH_CHART_COUNT];
static int32_t            s_ch_y_min[CH_CHART_COUNT];
static int32_t            s_ch_y_max[CH_CHART_COUNT];
static bool               s_ch_has_data[CH_CHART_COUNT];

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
    lv_obj_set_size(btn, 280, 70);
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

    create_ctrl_button(parent, LV_SYMBOL_PLAY " Start",       lv_color_hex(0x2E7D32), CTRL_ACTION_START);
    create_ctrl_button(parent, LV_SYMBOL_STOP " Stop",        lv_color_hex(0xC62828), CTRL_ACTION_STOP);
    create_ctrl_button(parent, LV_SYMBOL_REFRESH " Reinit",   lv_color_hex(0x1565C0), CTRL_ACTION_REINIT);
    create_ctrl_button(parent, LV_SYMBOL_POWER " Off",        lv_color_hex(0xE65100), CTRL_ACTION_OFF);
    create_ctrl_button(parent, LV_SYMBOL_CLOSE " Fault Clear",lv_color_hex(0x6A1B9A), CTRL_ACTION_FAULT_CLEAR);
}

/* ── Time Tab ────────────────────────────────────────── */

static void create_time_tab(lv_obj_t *parent) {
    s_time_table = lv_table_create(parent);
    lv_table_set_column_count(s_time_table, 2);
    lv_table_set_row_count(s_time_table, 5);
    lv_table_set_column_width(s_time_table, 0, 430);
    lv_table_set_column_width(s_time_table, 1, 250);

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
    lv_table_set_column_width(s_led_table, 0, 430);
    lv_table_set_column_width(s_led_table, 1, 250);

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
    lv_table_set_column_width(s_relay_table, 0, 430);
    lv_table_set_column_width(s_relay_table, 1, 250);

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
    lv_table_set_column_width(s_system_table, 0, 470);
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
    lv_table_set_column_width(s_measure_table, 0, 360);
    lv_table_set_column_width(s_measure_table, 1, 170);
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
    lv_table_set_column_width(s_state_table, 0, 430);
    lv_table_set_column_width(s_state_table, 1, 250);

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

/* ── Chart Tab (cold-head 2×3 grid) ──────────────────── */

static lv_obj_t *create_mini_chart(lv_obj_t *parent, const char *title,
                                   lv_color_t color, int idx)
{
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_size(cell, 335, 130);
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cell, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(cell, 2, LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(cell, lv_color_hex(0xDDDDDD), LV_PART_MAIN);
    lv_obj_set_style_radius(cell, 6, LV_PART_MAIN);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cell);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x003366), LV_PART_MAIN);

    lv_obj_t *chart = lv_chart_create(cell);
    lv_obj_set_size(chart, 320, 95);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, CH_POINT_COUNT);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(chart, 3, 5);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);

    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_line_color(chart, lv_color_hex(0xEEEEEE), LV_PART_MAIN);
    lv_obj_set_style_border_width(chart, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(chart, lv_color_hex(0xCCCCCC), LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0xFAFAFA), LV_PART_MAIN);

    s_ch_charts[idx] = chart;
    s_ch_series[idx] = lv_chart_add_series(chart, color, LV_CHART_AXIS_PRIMARY_Y);
    s_ch_has_data[idx] = false;

    return cell;
}

static void create_chart_tab(lv_obj_t *parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(parent, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(parent, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(parent, 6, LV_PART_MAIN);

    create_mini_chart(parent, "Cold Head Temp (K)",
                      lv_color_hex(0xFF6600), CH_CHART_TEMP_K);
    create_mini_chart(parent, "Cooling Rate",
                      lv_color_hex(0x2196F3), CH_CHART_COOLING_RATE);
    create_mini_chart(parent, "Cooldown %",
                      lv_color_hex(0x4CAF50), CH_CHART_COOLDOWN_PCT);
    create_mini_chart(parent, "\xCE\x94T Below Ambient (\xC2\xB0""C)",
                      lv_color_hex(0x9C27B0), CH_CHART_DELTA_AMBIENT);
    create_mini_chart(parent, "Cold Head Voltage (V)",
                      lv_color_hex(0xF44336), CH_CHART_VOLTAGE);
    create_mini_chart(parent, "Cold Head Current (A)",
                      lv_color_hex(0x009688), CH_CHART_CURRENT);
}

/* ── ESP-NOW Tab ─────────────────────────────────────── */

static lv_obj_t *s_espnow_label;
static lv_obj_t *s_espnow_status_badge;

static void create_espnow_tab(lv_obj_t *parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(parent, 12, LV_PART_MAIN);

    s_espnow_label = lv_label_create(parent);
    lv_label_set_long_mode(s_espnow_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_espnow_label, 670);
    lv_label_set_text(s_espnow_label, "Waiting for ESP-NOW data...");
    lv_obj_set_style_text_color(s_espnow_label, lv_color_hex(0x333333), LV_PART_MAIN);
}

/* ── Public API ──────────────────────────────────────── */

void dashboard_init(void) {
    init_stub_data();

    lv_obj_t *tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tv, LV_DIR_LEFT);
    lv_tabview_set_tab_bar_size(tv, 90);
    lv_obj_set_size(tv, 800, 480);

    lv_obj_t *tab_bar = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(tab_bar, lv_color_hex(0x003366), LV_PART_MAIN);
    lv_obj_set_style_text_color(tab_bar, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_side(tab_bar, LV_BORDER_SIDE_RIGHT, LV_PART_ITEMS);
    lv_obj_set_style_border_width(tab_bar, 3,
                                 (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_CHECKED));
    lv_obj_set_style_border_color(tab_bar, lv_color_hex(0x66BBFF),
                                  (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_CHECKED));
    lv_obj_set_style_text_align(tab_bar, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);

    lv_obj_t *tab_ctrl    = lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS " Control");
    lv_obj_t *tab_time    = lv_tabview_add_tab(tv, LV_SYMBOL_BELL " Time");
    lv_obj_t *tab_led     = lv_tabview_add_tab(tv, LV_SYMBOL_EYE_OPEN " LED");
    lv_obj_t *tab_relay   = lv_tabview_add_tab(tv, LV_SYMBOL_SHUFFLE " Relay");
    lv_obj_t *tab_system  = lv_tabview_add_tab(tv, LV_SYMBOL_WARNING " System");
    lv_obj_t *tab_measure = lv_tabview_add_tab(tv, LV_SYMBOL_GPS " Measure");
    lv_obj_t *tab_state   = lv_tabview_add_tab(tv, LV_SYMBOL_LIST " State");
    lv_obj_t *tab_chart   = lv_tabview_add_tab(tv, LV_SYMBOL_CHARGE " Chart");
    lv_obj_t *tab_espnow  = lv_tabview_add_tab(tv, LV_SYMBOL_WIFI " ESP-NOW");

    create_control_tab(tab_ctrl);
    create_time_tab(tab_time);
    create_led_tab(tab_led);
    create_relay_tab(tab_relay);
    create_system_tab(tab_system);
    create_measure_tab(tab_measure);
    create_state_tab(tab_state);
    create_chart_tab(tab_chart);
    create_espnow_tab(tab_espnow);

    s_espnow_status_badge = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_espnow_status_badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(s_espnow_status_badge, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_right(s_espnow_status_badge, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_top(s_espnow_status_badge, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(s_espnow_status_badge, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(s_espnow_status_badge, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_espnow_status_badge, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_espnow_status_badge, LV_OPA_90, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_espnow_status_badge, 0, LV_PART_MAIN);
    lv_obj_align(s_espnow_status_badge, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_clear_flag(s_espnow_status_badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *status_lbl = lv_label_create(s_espnow_status_badge);
    lv_label_set_text(status_lbl, "ESP-NOW: No Signal");
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

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

void dashboard_set_espnow_status(bool connected) {
    if (!s_espnow_status_badge) return;
    lv_obj_t *lbl = lv_obj_get_child(s_espnow_status_badge, 0);
    if (connected) {
        lv_obj_set_style_bg_color(s_espnow_status_badge, lv_color_hex(0x2E7D32), LV_PART_MAIN);
        lv_label_set_text(lbl, "ESP-NOW: Connected");
    } else {
        lv_obj_set_style_bg_color(s_espnow_status_badge, lv_color_hex(0x666666), LV_PART_MAIN);
        lv_label_set_text(lbl, "ESP-NOW: No Signal");
    }
}

void dashboard_set_espnow_message(const char *msg) {
    if (s_espnow_label) {
        lv_label_set_text(s_espnow_label, msg);
    }
}

void dashboard_add_cold_head_reading(cold_head_chart_t chart, float value) {
    int idx = (int)chart;
    if (idx < 0 || idx >= CH_CHART_COUNT || !s_ch_charts[idx]) return;

    int32_t val = (int32_t)(value * 10.0f);

    if (!s_ch_has_data[idx]) {
        s_ch_y_min[idx] = val;
        s_ch_y_max[idx] = val;
        s_ch_has_data[idx] = true;
    } else {
        if (val < s_ch_y_min[idx]) s_ch_y_min[idx] = val;
        if (val > s_ch_y_max[idx]) s_ch_y_max[idx] = val;
    }

    int32_t range = s_ch_y_max[idx] - s_ch_y_min[idx];
    int32_t pad = range / 10;
    if (pad < 10) pad = 10;

    lv_chart_set_axis_range(s_ch_charts[idx], LV_CHART_AXIS_PRIMARY_Y,
                            s_ch_y_min[idx] - pad, s_ch_y_max[idx] + pad);
    lv_chart_set_next_value(s_ch_charts[idx], s_ch_series[idx], val);
}
