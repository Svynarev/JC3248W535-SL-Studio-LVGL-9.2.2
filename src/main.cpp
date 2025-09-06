#include <lvgl.h>
#include <Arduino.h>
#include "pincfg.h"
#include "dispcfg.h"
#include "AXS15231B_touch.h"
#include <Arduino_GFX_Library.h>
#include "ui/ui.h"

// ================= Display & GFX =================
static Arduino_ESP32QSPI bus(TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
static Arduino_AXS15231B g(&bus, GFX_NOT_DEFINED, 0, false, TFT_res_W, TFT_res_H);
static Arduino_Canvas gfx(TFT_res_W, TFT_res_H, &g, 0, 0, TFT_rot);

// ================= Touch =================
AXS15231B_Touch touch(Touch_SCL, Touch_SDA, Touch_INT, Touch_ADDR, TFT_rot);

// ================= Backlight PWM =================
#define BL_CHANNEL   0      // LEDC канал (0–15)
#define BL_FREQ      5000   // Частота ШИМ
#define BL_RES       8      // Разрядность (0–255)

void setBacklight(uint8_t brightness) {
    ledcWrite(BL_CHANNEL, brightness);  // 0 = выкл, 255 = макс
}

// ================= LVGL Tick =================
uint32_t millis_cb(void) {
    return millis();
}

// ================= LVGL Flush =================
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);
    gfx.draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_disp_flush_ready(disp);
}

// ================= LVGL Touch =================
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t x, y;
    if (touch.touched()) {
        touch.readData(&x, &y);
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// Global variables for smooth brightness transition
int32_t current_brightness = 128;
int32_t target_brightness = 128;
const float FADE_MULTIPLIER = 0.1; // How much to change brightness each loop
const int32_t MIN_FADE_STEP = 1;

// ================= Slider Event Handler =================
// The function that will be called when the slider value changes
void slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
    // Update the target brightness, the main loop will handle the fade
    target_brightness = lv_slider_get_value(slider);
}

// ================= Setup =================
void setup() {
    Serial.begin(115200);
    delay(1000);

    // Init display with 50 MHz QSPI
    if (!gfx.begin(50000000UL)) {
        while (1);
    }
    gfx.fillScreen(BLACK);

    // Init backlight (PWM)
    ledcSetup(BL_CHANNEL, BL_FREQ, BL_RES);
    ledcAttachPin(TFT_BL, BL_CHANNEL);
    
    // Init touch
    if (!touch.begin()) {
        while (1);
    }
    touch.enOffsetCorrection(true);
    touch.setOffsets(Touch_X_min, Touch_X_max, TFT_res_W - 1,
                     Touch_Y_min, Touch_Y_max, TFT_res_H - 1);

    // Init LVGL
    lv_init();
    lv_tick_set_cb(millis_cb);

    // Init LVGL display buffer (a 60-line buffer)
    static lv_color_t buf[TFT_res_W * 60];
    lv_display_t *disp = lv_display_create(TFT_res_W, TFT_res_H);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf, NULL,
                           sizeof(buf) / sizeof(lv_color_t),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Init input device (touch)
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // Init UI (SquareLine)
    ui_init();

    // Set up the backlight slider
    // Set the value range and initial position for the slider
    lv_slider_set_range(ui_Slider1, 10, 255);
    lv_slider_set_value(ui_Slider1, 128, LV_ANIM_ON);

    // Connect the event handler to the slider
    lv_obj_add_event_cb(ui_Slider1, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Set the initial brightness
    setBacklight(lv_slider_get_value(ui_Slider1));
}

// ================= Loop =================
void loop() {
    // Smoothly adjust brightness towards the target value
    if (current_brightness != target_brightness) {
        int32_t diff = target_brightness - current_brightness;
        int32_t step = abs(diff) * FADE_MULTIPLIER;
        if (step < MIN_FADE_STEP) {
            step = MIN_FADE_STEP;
        }

        if (diff > 0) {
            current_brightness += step;
            if (current_brightness > target_brightness) {
                current_brightness = target_brightness;
            }
        } else {
            current_brightness -= step;
            if (current_brightness < target_brightness) {
                current_brightness = target_brightness;
            }
        }
        setBacklight(current_brightness);
    }

    lv_timer_handler();  // LVGL update (recommended API)
    gfx.flush();         // output Canvas to display
    delay(1);            // a small delay for stability
}
