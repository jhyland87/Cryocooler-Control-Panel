#include "console_log.h"

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/* ── Ring buffer ─────────────────────────────────────────────────────────── */

static char              s_lines[CONSOLE_LOG_LINE_COUNT][CONSOLE_LOG_LINE_MAX];
static uint32_t          s_write_idx = 0u;   ///< Next slot to write into
static uint32_t          s_read_idx  = 0u;   ///< Next slot to drain
static uint32_t          s_count     = 0u;   ///< Lines queued, not yet drained
static SemaphoreHandle_t s_mutex     = NULL;

/* ── Public API ──────────────────────────────────────────────────────────── */

void console_log_init(void) {
    s_mutex = xSemaphoreCreateMutex();
}

void console_log_push(const char *text, uint32_t len) {
    if (!s_mutex || !text || len == 0u) return;

    /* Clamp to buffer capacity (leave room for null terminator). */
    if (len >= CONSOLE_LOG_LINE_MAX) {
        len = CONSOLE_LOG_LINE_MAX - 1u;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;

    memcpy(s_lines[s_write_idx], text, len);
    s_lines[s_write_idx][len] = '\0';

    s_write_idx = (s_write_idx + 1u) % CONSOLE_LOG_LINE_COUNT;

    if (s_count < CONSOLE_LOG_LINE_COUNT) {
        s_count++;
    } else {
        /* Buffer full: the oldest entry was just overwritten, advance the
         * read pointer so it points at the next-oldest surviving entry.    */
        s_read_idx = (s_read_idx + 1u) % CONSOLE_LOG_LINE_COUNT;
    }

    xSemaphoreGive(s_mutex);
}

bool console_log_drain(void (*line_cb)(const char *line)) {
    if (!s_mutex || !line_cb) return false;

    bool had_lines = false;

    for (;;) {
        char     tmp[CONSOLE_LOG_LINE_MAX];
        bool     got = false;

        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            if (s_count > 0u) {
                memcpy(tmp, s_lines[s_read_idx], CONSOLE_LOG_LINE_MAX);
                s_read_idx = (s_read_idx + 1u) % CONSOLE_LOG_LINE_COUNT;
                s_count--;
                got = true;
            }
            xSemaphoreGive(s_mutex);
        }

        if (!got) break;

        line_cb(tmp);
        had_lines = true;
    }

    return had_lines;
}
