#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of a single console line (including null terminator). */
#define CONSOLE_LOG_LINE_MAX    200u

/** Number of lines kept in the ring buffer before the oldest is overwritten. */
#define CONSOLE_LOG_LINE_COUNT   64u

/**
 * Initialise the ring buffer and its FreeRTOS mutex.
 * Must be called once from app_main() before any task that calls
 * console_log_push() or console_log_drain().
 */
void console_log_init(void);

/**
 * Enqueue a text chunk received from the master ESP over ESP-NOW.
 * The text must NOT include the leading 0x01 marker byte — strip it before
 * calling.  @p len bytes are copied; no null terminator is required in the
 * source buffer.
 *
 * Thread-safe.  Safe to call from the WiFi task (Core 0) inside
 * espnow_recv_cb().  If the ring buffer is full, the oldest line is
 * silently discarded to make room.
 */
void console_log_push(const char *text, uint32_t len);

/**
 * Drain all queued lines into the LVGL task by invoking @p line_cb once per
 * line in FIFO order.  Returns true if at least one line was drained.
 *
 * Must be called from within the LVGL task (ui_task) while the LVGL API lock
 * is already held, so that @p line_cb can safely call LVGL functions.
 */
bool console_log_drain(void (*line_cb)(const char *line));

#ifdef __cplusplus
}
#endif
