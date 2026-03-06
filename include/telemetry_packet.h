#ifndef TELEMETRY_PACKET_H
#define TELEMETRY_PACKET_H

#include <stdint.h>

struct __attribute__((packed)) TelemetryPacket {
    // ── Timestamp ─────────────────────────────────────────────────────────
    int64_t  timestamp_epoch;               ///< Unix time (UTC); 0 before SNTP sync

    // ── State machine ──────────────────────────────────────────────────────
    int8_t   state_id;                      ///< state_machine::State cast to int8_t
    uint32_t status_on_duration_ms;         ///< cumulative on-state duration in ms
    uint32_t status_time_in_state_ms;       ///< time in current state in ms
    uint8_t  faults_count_10m;              ///< fault entries in last 10 min
    uint8_t  faults_count_30m;              ///< fault entries in last 30 min
    uint8_t  faults_count_60m;              ///< fault entries in last 60 min

    // ── Cold head ─────────────────────────────────────────────────────────
    float    cold_head_temp_k;
    float    cold_head_temp_c;
    float    cold_head_ambient_temp_c;
    float    cold_head_delta_below_ambient_c;
    float    cold_head_cooling_rate;        ///< K/min; positive = cooling
    float    cold_head_cooldown_pct;        ///< 0–100 %
    float    cold_head_voltage_v;
    float    cold_head_current_a;

    // ── Amplifier ─────────────────────────────────────────────────────────
    float    amplifier_voltage_v;
    float    amplifier_current_a;

    // ── System supply ──────────────────────────────────────────────────────
    float    system_voltage_v;
    float    system_current_a;
    float    system_power_w;

    // ── IMU ───────────────────────────────────────────────────────────────
    float    imu_roll_deg;
    float    imu_pitch_deg;
    float    imu_yaw_deg;
    float    imu_accel_mag;
    float    imu_temp_c;
    float    imu_x;
    float    imu_y;
    float    imu_z;
    uint8_t  imu_motion;                    ///< 1 = motion/overstroke detected

    // ── Cooling loop ──────────────────────────────────────────────────────
    float    cooling_temp_c;
    float    cooling_flow_rate_lpm;
    uint16_t cooling_fan_speed;             ///< PWM duty (0–100)
    uint16_t cooling_fan_rpm;
    uint8_t  cooling_status;               ///< 1 = cooling enabled
    uint8_t  cooling_pump_on;              ///< 1 = pump active

    // ── Tracking scores ───────────────────────────────────────────────────
    float    score_fan_speed;
    float    score_coolant_temp;
    float    score_coolant_flow;
    float    score_worst;

    // ── Indicators ────────────────────────────────────────────────────────
    uint8_t  indicator_fault;              ///< 1 = FAULT LED on
    uint8_t  indicator_ready;              ///< 1 = READY LED on
};

#endif
