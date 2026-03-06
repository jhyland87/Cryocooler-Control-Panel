#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_timer.h>
#include <esp_idf_version.h>
#include <esp_mac.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <nvs_flash.h>
#include <esp_heap_caps.h>
#include "telemetry_packet.h"
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <string.h>
#include "lcd_config.h"
#include <lvgl.h>
#include "ui/ui.h"
#include "ui/dashboard.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#define I2C_MASTER_SCL_IO           9       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           8       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0       /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define GPIO_INPUT_IO_4    4
#define GPIO_INPUT_PIN_SEL  1ULL<<GPIO_INPUT_IO_4

static esp_lcd_panel_handle_t lcd_handle = nullptr;
static SemaphoreHandle_t sem_vsync_end;
static SemaphoreHandle_t sem_gui_ready;
static _lock_t lvgl_api_lock;
esp_lcd_touch_handle_t touch_handle = NULL;

static void i2c_initialize()
{
    i2c_master_bus_config_t i2c_mst_config;
    memset(&i2c_mst_config,0,sizeof(i2c_mst_config));
    i2c_mst_config.clk_source = I2C_CLK_SRC_DEFAULT;
    i2c_mst_config.i2c_port = (i2c_port_num_t)I2C_MASTER_NUM;
    i2c_mst_config.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_IO;
    i2c_mst_config.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_IO;
    i2c_mst_config.glitch_ignore_cnt = 7;
    i2c_mst_config.flags.enable_internal_pullup = 1;
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus));
}


static bool lcd_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *event_data, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;

    if (xSemaphoreTakeFromISR(sem_gui_ready, &high_task_awoken) == pdTRUE) {
        xSemaphoreGiveFromISR(sem_vsync_end, &high_task_awoken);
    }

    return high_task_awoken == pdTRUE;
}
static void lcd_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(2);
}
static void lvgl_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lcd_handle;
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    xSemaphoreGive(sem_gui_ready);
    xSemaphoreTake(sem_vsync_end, portMAX_DELAY);

    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
    lv_display_flush_ready(disp);
}


static void lcd_initialize() {
    printf("Free internal RAM: %lu bytes\n",
           (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    sem_vsync_end = xSemaphoreCreateBinary();
    assert(sem_vsync_end);
    sem_gui_ready = xSemaphoreCreateBinary();
    assert(sem_gui_ready);

    #if LCD_PIN_NUM_BCKL >= 0
    gpio_config_t bk_gpio_config;
    memset(&bk_gpio_config,0,sizeof(gpio_config_t));
    bk_gpio_config.mode = GPIO_MODE_OUTPUT;
    bk_gpio_config.pin_bit_mask = 1ULL << LCD_PIN_NUM_BCKL;
    ESP_ERROR_CHECK(gpio_config((gpio_num_t)LCD_PIN_NUM_BCKL,&bk_gpio_config));
    gpio_set_level((gpio_num_t)LCD_PIN_NUM_BCKL, LCD_BCKL_OFF_LEVEL);
    #endif

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_rgb_panel_config_t panel_config;
    memset(&panel_config,0,sizeof(esp_lcd_rgb_panel_config_t));

        panel_config.data_width = 16; // RGB565 in parallel mode, thus 16bit in width
        //panel_config.dma_burst_size = 64;
        panel_config.num_fbs = 1,
        panel_config.clk_src = LCD_CLK_SRC_DEFAULT,
        panel_config.disp_gpio_num = -1,
        panel_config.pclk_gpio_num = LCD_PIN_NUM_CLK,
        panel_config.vsync_gpio_num = LCD_PIN_NUM_VSYNC,
        panel_config.hsync_gpio_num = LCD_PIN_NUM_HSYNC,
        panel_config.de_gpio_num = LCD_PIN_NUM_DE,
        panel_config.data_gpio_nums[0]=LCD_PIN_NUM_D00;
        panel_config.data_gpio_nums[1]=LCD_PIN_NUM_D01;
        panel_config.data_gpio_nums[2]=LCD_PIN_NUM_D02;
        panel_config.data_gpio_nums[3]=LCD_PIN_NUM_D03;
        panel_config.data_gpio_nums[4]=LCD_PIN_NUM_D04;
        panel_config.data_gpio_nums[5]=LCD_PIN_NUM_D05;
        panel_config.data_gpio_nums[6]=LCD_PIN_NUM_D06;
        panel_config.data_gpio_nums[7]=LCD_PIN_NUM_D07;
        panel_config.data_gpio_nums[8]=LCD_PIN_NUM_D08;
        panel_config.data_gpio_nums[9]=LCD_PIN_NUM_D09;
        panel_config.data_gpio_nums[10]=LCD_PIN_NUM_D10;
        panel_config.data_gpio_nums[11]=LCD_PIN_NUM_D11;
        panel_config.data_gpio_nums[12]=LCD_PIN_NUM_D12;
        panel_config.data_gpio_nums[13]=LCD_PIN_NUM_D13;
        panel_config.data_gpio_nums[14]=LCD_PIN_NUM_D14;
        panel_config.data_gpio_nums[15]=LCD_PIN_NUM_D15;

        memset(&panel_config.timings,0,sizeof(esp_lcd_rgb_timing_t));

        panel_config.timings.pclk_hz = LCD_PIXEL_CLOCK_HZ;
        panel_config.timings.h_res = LCD_HRES;
        panel_config.timings.v_res = LCD_VRES;
        panel_config.timings.hsync_back_porch = LCD_HSYNC_BACK_PORCH;
        panel_config.timings.hsync_front_porch = LCD_HSYNC_FRONT_PORCH;
        panel_config.timings.hsync_pulse_width = LCD_HSYNC_PULSE_WIDTH;
        panel_config.timings.vsync_back_porch = LCD_VSYNC_BACK_PORCH;
        panel_config.timings.vsync_front_porch = LCD_VSYNC_FRONT_PORCH;
        panel_config.timings.vsync_pulse_width = LCD_VSYNC_PULSE_WIDTH;
        panel_config.timings.flags.pclk_active_neg = true;
        panel_config.timings.flags.hsync_idle_low = false;
        panel_config.timings.flags.pclk_idle_high = LCD_CLK_IDLE_HIGH;
        panel_config.timings.flags.de_idle_high = LCD_DE_IDLE_HIGH;
        panel_config.timings.flags.vsync_idle_low = false;
        panel_config.flags.bb_invalidate_cache = true;
        panel_config.flags.disp_active_low = false;
        panel_config.flags.double_fb = false;
        panel_config.flags.no_fb = false;
        panel_config.flags.refresh_on_demand = false;
        panel_config.flags.fb_in_psram = true; // allocate frame buffer in PSRAM
        //panel_config.sram_trans_align = 4;
        //panel_config.psram_trans_align = 64;
        panel_config.num_fbs = 2;
        panel_config.bounce_buffer_size_px = LCD_HRES * (LCD_VRES / 20);
        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
        lcd_handle = panel_handle;
        #if LCD_PIN_NUM_BCKL >= 0
        gpio_set_level((gpio_num_t)LCD_PIN_NUM_BCKL, LCD_BCKL_ON_LEVEL);
        #endif
        lv_init();
        // create a lvgl display
        lv_display_t *display = lv_display_create(LCD_HRES, LCD_VRES);

        // set color depth
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        void *buf1 = NULL;
        size_t draw_buffer_sz = LCD_HRES * 240 * sizeof(lv_color16_t);

        buf1 = (lv_color_t *)heap_caps_malloc(draw_buffer_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        assert(buf1);
        lv_display_set_buffers(display, buf1, NULL, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);

                // set the callback which can copy the rendered image to an area of the display
        lv_display_set_flush_cb(display, lvgl_flush);
        esp_lcd_rgb_panel_event_callbacks_t cbs;
        memset(&cbs,0,sizeof(esp_lcd_rgb_panel_event_callbacks_t));
        cbs.on_vsync = lcd_vsync_event;
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, display));
        // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
        esp_timer_create_args_t lvgl_tick_timer_args;
        memset(&lvgl_tick_timer_args,0,sizeof(esp_timer_create_args_t));
        lvgl_tick_timer_args.callback = &lcd_increase_lvgl_tick;
        lvgl_tick_timer_args.name = "lvgl_tick";
        esp_timer_handle_t lvgl_tick_timer = NULL;
        ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
        ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, 2 * 1000));
        _lock_acquire(&lvgl_api_lock);
        ui_init();
        _lock_release(&lvgl_api_lock);

}
static void my_input_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    uint16_t x[5],y[5],s[5];
    uint8_t count;
    if(esp_lcd_touch_get_coordinates(touch_handle,x,y,s,&count,5)) {
        data->point.x =x[0];
        data->point.y = y[0];
        data->state = LV_INDEV_STATE_PRESSED;
        return;
    }
    data->state = LV_INDEV_STATE_RELEASED;
}
static void touch_reset()
{
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_master_get_bus_handle((i2c_port_num_t)I2C_MASTER_NUM,&bus));
    i2c_master_dev_handle_t i2c=NULL;
    i2c_device_config_t dev_cfg;
    memset(&dev_cfg,0,sizeof(dev_cfg));
    dev_cfg.scl_speed_hz = 200*1000;
    dev_cfg.device_address = 0x24;
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg,&i2c));
    uint8_t write_buf = 0x01;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c,&write_buf,1,I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(i2c));
    dev_cfg.device_address = 0x38;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg,&i2c));
    // Reset the touch screen. It is recommended to reset the touch screen before using it.
    write_buf = 0x2C;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c,&write_buf,1,I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
    esp_rom_delay_us(100 * 1000);
    gpio_set_level((gpio_num_t)GPIO_INPUT_IO_4, 0);
    esp_rom_delay_us(100 * 1000);
    write_buf = 0x2E;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c,&write_buf,1,I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS));
    esp_rom_delay_us(200 * 1000);
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(i2c));
}


static void touch_initialize() {
    touch_reset();
    esp_lcd_panel_io_i2c_config_t tio_cfg;
    esp_lcd_panel_io_handle_t tio_handle;
    esp_lcd_touch_config_t tp_cfg;
    i2c_master_bus_handle_t i2c_handle;

    memset(&tio_cfg,0,sizeof(tio_cfg));
    tio_cfg.dev_addr = ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS;
    tio_cfg.control_phase_bytes = 1;
    tio_cfg.dc_bit_offset = 0;
    tio_cfg.lcd_cmd_bits = 16;
    tio_cfg.lcd_param_bits = 0;
    tio_cfg.flags.disable_control_phase = 1;
    tio_cfg.flags.dc_low_on_data = 0;
    tio_cfg.on_color_trans_done = NULL;
    tio_cfg.scl_speed_hz = 200*1000;
    tio_cfg.user_ctx = NULL;
    ESP_ERROR_CHECK(i2c_master_get_bus_handle(I2C_MASTER_NUM,&i2c_handle)) ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_handle, &tio_cfg,&tio_handle));
    memset(&tp_cfg,0,sizeof(tp_cfg));
    tp_cfg.x_max = LCD_HRES;
    tp_cfg.y_max = LCD_VRES;
    tp_cfg.rst_gpio_num = (gpio_num_t)-1;
    tp_cfg.int_gpio_num = (gpio_num_t)-1;
    tp_cfg.levels.reset = 0;
    tp_cfg.levels.interrupt = 0;
    tp_cfg.flags.swap_xy = 0;
    tp_cfg.flags.mirror_x = 0;
    tp_cfg.flags.mirror_y = 0;

    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tio_handle,&tp_cfg,&touch_handle));
    lv_indev_t * indev = lv_indev_create();        /* Create input device connected to Default Display. */
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);   /* Touch pad is a pointer-like device. */
    lv_indev_set_read_cb(indev, my_input_read);    /* Set driver function. */

}
/* ── ESP-NOW ─────────────────────────────────────────── */

static TelemetryPacket    s_telem_pkt;
static uint8_t            s_telem_src_mac[6];
static volatile bool      s_telem_ready;
static volatile bool      s_peer_registered;
static SemaphoreHandle_t  s_telem_mutex;
static volatile int64_t   s_espnow_last_recv_us;

#define ESPNOW_TIMEOUT_US  (5 * 1000 * 1000)
#define ESPNOW_DISPLAY_MAX 1024

static char *s_espnow_display;

static void espnow_recv_cb(const esp_now_recv_info_t *info,
                           const uint8_t *data, int len)
{
    printf("espnow_recv_cb: len: %d, sizeof(TelemetryPacket): %d\n", len, sizeof(TelemetryPacket));
    if (len != sizeof(TelemetryPacket)) return;

    if (xSemaphoreTake(s_telem_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        memcpy(&s_telem_pkt, data, sizeof(TelemetryPacket));
        memcpy(s_telem_src_mac, info->src_addr, 6);
        s_telem_ready = true;
        s_espnow_last_recv_us = esp_timer_get_time();
        xSemaphoreGive(s_telem_mutex);
    }
}

static void espnow_send_command(dashboard_ctrl_action_t action) {
    printf("espnow_send_command: %d\n", action);
}


static void espnow_initialize(void)
{
    s_espnow_display = (char *)heap_caps_malloc(ESPNOW_DISPLAY_MAX, MALLOC_CAP_SPIRAM);
    assert(s_espnow_display);

    s_telem_mutex = xSemaphoreCreateMutex();
    assert(s_telem_mutex);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    printf("ESP-NOW receiver initialized (pkt size=%u)\n",
           (unsigned)sizeof(TelemetryPacket));
}

/* ── Telemetry struct → dashboard mapping ────────────── */

static void ms_to_hhmmss(uint32_t ms, char *buf, size_t sz)
{
    uint32_t total_s = ms / 1000;
    uint32_t h = total_s / 3600;
    uint32_t m = (total_s % 3600) / 60;
    uint32_t s = total_s % 60;
    snprintf(buf, sz, "%02lu:%02lu:%02lu",
             (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

static const char *state_id_to_name(int8_t id)
{
    switch (id) {
        case 0: return "Initialize";
        case 1: return "Standby";
        case 2: return "Cooldown";
        case 3: return "Running";
        case 4: return "Warmup";
        case 5: return "Fault";
        case 6: return "Shutdown";
        default: return "Unknown";
    }
}

static void format_display(const TelemetryPacket *p, const uint8_t *mac)
{
    snprintf(s_espnow_display, ESPNOW_DISPLAY_MAX,
        "From: %02X:%02X:%02X:%02X:%02X:%02X\n"
        "State: %s  On: %lums  InState: %lums\n"
        "Cold Head: %.1fK / %.1f\xC2\xB0""C  Ambient: %.1f\xC2\xB0""C\n"
        "Cooling Rate: %.2f  Cooldown: %.1f%%\n"
        "Amp: %.2fV %.2fA  Sys: %.2fV %.2fA %.1fW\n"
        "IMU: R%.1f P%.1f Y%.1f  Motion:%u\n"
        "Cooling: %.1f\xC2\xB0""C  %.2fLPM  Fan:%u/%uRPM\n"
        "Scores: fan=%.1f temp=%.1f flow=%.1f worst=%.1f\n"
        "Fault:%u  Ready:%u",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
        state_id_to_name(p->state_id),
        (unsigned long)p->status_on_duration_ms,
        (unsigned long)p->status_time_in_state_ms,
        (double)p->cold_head_temp_k, (double)p->cold_head_temp_c,
        (double)p->cold_head_ambient_temp_c,
        (double)p->cold_head_cooling_rate, (double)p->cold_head_cooldown_pct,
        (double)p->amplifier_voltage_v, (double)p->amplifier_current_a,
        (double)p->system_voltage_v, (double)p->system_current_a,
        (double)p->system_power_w,
        (double)p->imu_roll_deg, (double)p->imu_pitch_deg,
        (double)p->imu_yaw_deg, p->imu_motion,
        (double)p->cooling_temp_c, (double)p->cooling_flow_rate_lpm,
        p->cooling_fan_speed, p->cooling_fan_rpm,
        (double)p->score_fan_speed, (double)p->score_coolant_temp,
        (double)p->score_coolant_flow, (double)p->score_worst,
        p->indicator_fault, p->indicator_ready);
}

static void apply_telemetry(const TelemetryPacket *p)
{
    dashboard_data_t *d = dashboard_get_data();

    ms_to_hhmmss(p->status_on_duration_ms,   d->time_cryo_running,     DASHBOARD_VALUE_BUF_SIZE);
    ms_to_hhmmss(p->status_time_in_state_ms, d->runtime_since_powerup, DASHBOARD_VALUE_BUF_SIZE);

    snprintf(d->state_machine_mode,  DASHBOARD_VALUE_BUF_SIZE, "%s", state_id_to_name(p->state_id));
    snprintf(d->state_machine_state, DASHBOARD_VALUE_BUF_SIZE, "State %d", p->state_id);

    d->status_led = p->indicator_ready != 0;
    d->bypass_led = p->indicator_fault != 0;

    d->cold_stage_narrow_temp = p->cold_head_temp_k;
    d->cold_stage_wide_temp   = p->cold_head_temp_c;
    d->ambient_temp           = p->cold_head_ambient_temp_c;
    d->cooler_rejection_temp  = p->cooling_temp_c;

    d->cooler_voltage_real = p->amplifier_voltage_v;
    d->cooler_current_real = p->amplifier_current_a;
    d->input_voltage       = p->system_voltage_v;
    d->cooler_power        = p->system_power_w;
    d->cooler_current_rms  = p->system_current_a;

    dashboard_add_cold_head_reading(CH_CHART_TEMP_K,        p->cold_head_temp_k);
    dashboard_add_cold_head_reading(CH_CHART_COOLING_RATE,  p->cold_head_cooling_rate);
    dashboard_add_cold_head_reading(CH_CHART_COOLDOWN_PCT,  p->cold_head_cooldown_pct);
    dashboard_add_cold_head_reading(CH_CHART_DELTA_AMBIENT, p->cold_head_delta_below_ambient_c);
    dashboard_add_cold_head_reading(CH_CHART_VOLTAGE,       p->cold_head_voltage_v);
    dashboard_add_cold_head_reading(CH_CHART_CURRENT,       p->cold_head_current_a);

    dashboard_mark_dirty();
}

/* ── UI task ─────────────────────────────────────────── */

static void ui_task(void *arg)
{
    while (1) {
        if (touch_handle != NULL) {
            esp_lcd_touch_read_data(touch_handle);
        }
        _lock_acquire(&lvgl_api_lock);
        lv_timer_handler();
        ui_tick();
        //printf("s_telem_ready: %d\n", s_telem_ready);
        if (s_telem_ready) {
            //printf("s_telem_ready: true\n");
            if (xSemaphoreTake(s_telem_mutex, 0) == pdTRUE) {
                TelemetryPacket local;
                uint8_t mac[6];
                memcpy(&local, &s_telem_pkt, sizeof(local));
                memcpy(mac, s_telem_src_mac, 6);
                s_telem_ready = false;
                xSemaphoreGive(s_telem_mutex);

                format_display(&local, mac);
                dashboard_set_espnow_message(s_espnow_display);
                dashboard_set_espnow_status(true);
                apply_telemetry(&local);
            }
        } else if (s_espnow_last_recv_us > 0 &&
                   (esp_timer_get_time() - s_espnow_last_recv_us) > ESPNOW_TIMEOUT_US) {
            dashboard_set_espnow_status(false);
        }

        _lock_release(&lvgl_api_lock);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

extern "C" void app_main() {
    printf("ESP-IDF version: %d.%d.%d\n",ESP_IDF_VERSION_MAJOR,ESP_IDF_VERSION_MINOR,ESP_IDF_VERSION_PATCH);

    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
    printf("WiFi STA MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    i2c_initialize();
    lcd_initialize();
    touch_initialize();
    espnow_initialize();

    dashboard_set_ctrl_callback(espnow_send_command);

    xTaskCreate(ui_task, "ui_task", 8192, NULL, 2, NULL);
}