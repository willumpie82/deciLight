#ifndef LED_H
#define LED_H

#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "web.h"

// External LED strip object
extern Adafruit_NeoPixel strip;

// LED functions
void led_init();
void led_rgb_chase();
void display_traffic_light(double dB_current);
void display_vu_meter(double dB_current);
void led_display(double dB_current);
void led_display_with_config(double dB_current, const Config& config);

// LED helper functions (subroutines)
uint32_t get_color_for_level(double dB_current, const Config& config);
void handle_traffic_light_display(double dB_current, const Config& config, uint32_t now);
void handle_vu_meter_display(double dB_current, const Config& config);

// Async handler (displays LED with current level)
void led_handle_async(double dB_current);

#endif // LED_H
