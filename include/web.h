#ifndef WEB_H
#define WEB_H

#include <WebServer.h>

// Configuration structure for runtime settings
struct Config {
  int display_mode;           // 0=TRAFFIC_LIGHT, 1=VU_METER
  float db_floor;
  float db_normal_switchover;   // Switchover from NORMAL to WARNING
  float db_warning_switchover;  // Switchover from WARNING to ALERT
  uint8_t led_brightness;  // 0-255
  uint32_t color_normal;   // 0xRRGGBB
  uint32_t color_warning;  // 0xRRGGBB
  uint32_t color_alert;    // 0xRRGGBB
  uint16_t decay_ms;       // How long to hold color after sound (0-3000ms)
  uint16_t response_ms;    // Min time between LED updates (0-500ms)
};

extern Config runtime_config;
extern WebServer web_server;

// Web server functions
void web_init();
void web_load_config();
void web_save_config();
void web_handle_root();
void web_handle_api_get();
void web_handle_api_set();
void web_handle_api_status();
void web_handle_not_found();

// Async handler (manages own timing)
void web_handle_async();

// Update current dB level (called from main loop)
void web_update_level(double dB_current);

#endif // WEB_H
