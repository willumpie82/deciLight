#include "led.h"
#include "config.h"
#include "web.h"
#include <cmath>

// Global LED strip object
Adafruit_NeoPixel strip(NUM_LEDS, DATA_PIN, NEO_GRB + NEO_KHZ800);

// Timing tracking for decay and response
static uint32_t last_led_update_time = 0;
static uint32_t last_color_update_time = 0;
static uint32_t last_displayed_color = 0;
static NoiseLevel last_noise_level = NORMAL;  // Green level

//
// INITIALIZE LED STRIP
//
void led_init() {
  log_i("LED: Init %d pixels on GPIO %d", NUM_LEDS, DATA_PIN);
  strip.begin();
  strip.show();
  strip.setBrightness(LED_BRIGHTNESS);
  
  // Clear all pixels
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
  log_i("LED: Ready");
}

//
// RGB CHASE SEQUENCE (LED INIT) - SIMPLIFIED
//
void led_rgb_chase() {
  log_i("LED: Quick startup indicator");
  
  // Quick pulse instead of chase
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, strip.Color(0, 100, 0));  // DIM GREEN
  }
  strip.show();
  delay(200);
  
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  strip.show();
  
  log_i("LED: Startup complete");
}

//
// TRAFFIC LIGHT MODE - All LEDs same color based on noise level
//
void display_traffic_light(double dB_current) {
  strip.setBrightness(runtime_config.led_brightness);
  
  double normal_switchover = runtime_config.db_normal_switchover;
  double warning_switchover = runtime_config.db_warning_switchover;
  log_i("traffic light (%.1f dB)", dB_current);

  uint32_t color;
  
  if (dB_current < normal_switchover) {
    color = runtime_config.color_normal;  // NORMAL - quiet
    log_i("show normal (%.1f dB < %.1f)", dB_current, normal_switchover);
  } else if (dB_current < warning_switchover) {
    color = runtime_config.color_warning;  // WARNING - normal
    log_i("show warning (%.1f dB < %.1f)", dB_current, warning_switchover);
  } else {
    color = runtime_config.color_alert;  // ALERT - loud
    log_i("show alert (%.1f dB >= %.1f)", dB_current, warning_switchover);
  }
  
  // Light all LEDs with the same color
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
}

//
// VU METER MODE - Gradient coloring with varying LED count
//
void display_vu_meter(double dB_current) {
  strip.setBrightness(runtime_config.led_brightness);
  
  double normal_switchover = runtime_config.db_normal_switchover;
  double warning_switchover = runtime_config.db_warning_switchover;
  
  // VU meter parameters - map dB range to LED count
  double dB_min = runtime_config.db_floor;
  double dB_max = 65.0;
  
  double dB_normalized = (dB_current - dB_min) / (dB_max - dB_min);
  dB_normalized = (dB_normalized < 0) ? 0 : (dB_normalized > 1) ? 1 : dB_normalized;
  
  int led_count = (int)(dB_normalized * NUM_LEDS);
  
  // Clear all LEDs first
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  
  // Determine base color based on dB switchover points
  uint32_t base_color;
  if (dB_current < normal_switchover) {
    base_color = runtime_config.color_normal;      // NORMAL
  } else if (dB_current < warning_switchover) {
    base_color = runtime_config.color_warning;    // WARNING
  } else {
    base_color = runtime_config.color_alert;      // ALERT
  }
  
  // Extract RGB components from base_color (format: 0xRRGGBB)
  uint8_t base_r = (base_color >> 16) & 0xFF;
  uint8_t base_g = (base_color >> 8) & 0xFF;
  uint8_t base_b = base_color & 0xFF;
  
  // Light up LEDs with same color, varying brightness by position
  for (int i = 0; i < led_count; i++) {
    float pos = (float)i / NUM_LEDS;  // 0.0 to 1.0
    
    // Add brightness variation across the LEDs
    float brightness_factor = 0.5 + 0.5 * pos;  // Ramp from 50% to 100%
    
    // Scale RGB components by brightness factor
    uint8_t r = (uint8_t)(base_r * brightness_factor);
    uint8_t g = (uint8_t)(base_g * brightness_factor);
    uint8_t b = (uint8_t)(base_b * brightness_factor);
    
    uint32_t color = strip.Color(r, g, b);
    strip.setPixelColor(i, color);
  }
  
  strip.show();
}

//
// UNIFIED DISPLAY FUNCTION
//
void led_display(double dB_current) {
  if (DISPLAY_MODE == 0) {
    display_traffic_light(dB_current);
  } else {
    display_vu_meter(dB_current);
  }
}

//
// DETERMINE COLOR BASED ON dB THRESHOLDS
//
uint32_t get_color_for_level(double dB_current, const Config& config) {
  if (dB_current < config.db_normal_switchover) {
    return config.color_normal;    // NORMAL - quiet
  } else if (dB_current < config.db_warning_switchover) {
    return config.color_warning;   // WARNING - normal
  } else {
    return config.color_alert;     // ALERT - loud
  }
}

//
// TRAFFIC LIGHT MODE WITH DECAY & RESPONSE THROTTLING
//
void handle_traffic_light_display(double dB_current, const Config& config, uint32_t now) {
  uint32_t new_color = get_color_for_level(dB_current, config);
  NoiseLevel new_noise_level = (dB_current < config.db_normal_switchover) ? NORMAL :
                               (dB_current < config.db_warning_switchover) ? WARNING : ALERT;
  
  // Handle decay: only allow escalation during decay period
  uint32_t time_since_color_change = now - last_color_update_time;
  if (config.decay_ms > 0 && time_since_color_change < config.decay_ms) {
    if (new_noise_level < last_noise_level) {
      // Trying to go quieter during decay - keep current color
      new_color = last_displayed_color;
    } else if (new_noise_level > last_noise_level) {
      // Escalating to higher level - update and reset timer
      last_noise_level = new_noise_level;
      last_color_update_time = now;
    }
  } else {
    // Decay expired or no decay - update normally
    if (new_color != last_displayed_color) {
      last_noise_level = new_noise_level;
      last_color_update_time = now;
    }
  }
  
  // Check response throttling (min update interval for color changes)
  if (new_color != last_displayed_color && config.response_ms > 0 && (now - last_led_update_time) < config.response_ms) {
    return;  // Not enough time passed, skip update
  }
  
  last_displayed_color = new_color;
  last_led_update_time = now;
  
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, new_color);
  }
  strip.show();
}

//
// VU METER MODE WITH BRIGHTNESS RAMP
//
void handle_vu_meter_display(double dB_current, const Config& config) {
  double dB_min = config.db_floor;
  double dB_max = 80.0;
  
  // Normalize dB to 0-1 range
  double dB_normalized = (dB_current - dB_min) / (dB_max - dB_min);
  dB_normalized = (dB_normalized < 0) ? 0 : (dB_normalized > 1) ? 1 : dB_normalized;
  
  int led_count = (int)(dB_normalized * NUM_LEDS);
  
  // Clear all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, 0);
  }
  
  // Get base color for current level
  uint32_t base_color = get_color_for_level(dB_current, config);
  
  // Extract RGB components (format: 0xRRGGBB)
  uint8_t base_r = (base_color >> 16) & 0xFF;
  uint8_t base_g = (base_color >> 8) & 0xFF;
  uint8_t base_b = base_color & 0xFF;
  
  // Light up LEDs with brightness ramp (50% to 100%)
  for (int i = 0; i < led_count; i++) {
    float brightness_factor = 0.5 + 0.5 * ((float)i / NUM_LEDS);
    
    uint8_t r = (uint8_t)(base_r * brightness_factor);
    uint8_t g = (uint8_t)(base_g * brightness_factor);
    uint8_t b = (uint8_t)(base_b * brightness_factor);
    
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  
  strip.show();
}

//
// DISPLAY WITH RUNTIME CONFIG - MAIN DISPATCHER
//
void led_display_with_config(double dB_current, const Config& config) {
  strip.setBrightness(config.led_brightness);
  
  uint32_t now = millis();
  
  if (config.display_mode == 0) {
    handle_traffic_light_display(dB_current, config, now);
  } else {
    handle_vu_meter_display(dB_current, config);
  }
  
  last_led_update_time = now;
}
