/*
 * NOISE TRAFFIC LIGHT
 * 
 * ESP32 Noise-based RGB Traffic Light
 * Adapted from deciLight by bbbenji
 * Original: https://github.com/bbbenji/deciLight
 * 
 * This monitors ambient sound level via I2S microphone and displays:
 * - GREEN: Noise is LOW (below threshold)
 * - YELLOW: Noise is MEDIUM (between thresholds)
 * - RED: Noise is HIGH (above threshold)
 * 
 * MODULAR ARCHITECTURE:
 * - config.h: All configuration constants and #defines
 * - led.h/led.cpp: LED control and display modes
 * - microphone.h/microphone.cpp: I2S microphone and audio processing
 * - web.h/web.cpp: WiFi AP and web configuration interface
 * - main.cpp: Setup and main loop
 */

#include <Arduino.h>
#include "config.h"
#include "led.h"
#include "microphone.h"
#include "web.h"



//
// SETUP
//
void setup() {
  delay(100);
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(500);  // Extra delay for USB CDC to initialize
  
  Serial.println("\n\n--- BOOT START ---");
  
  log_e("\n=== NOISE TRAFFIC LIGHT STARTING ===");
  log_i("CPU: %d MHz, PSRAM: %d bytes", ESP.getCpuFreqMHz(), ESP.getFreePsram());

  // LED setup
  led_init();
  //delay(500);
  //led_rgb_chase();

  // Microphone setup
  mic_init();
  
  // Web interface setup (WiFi AP mode)
  web_init();
  
  log_e("=== SYSTEM READY ===\n");
}

//
// MAIN LOOP
//
void loop() {
  static unsigned long loop_count = 0;
  
  // Each async handler manages its own timing
  web_handle_async();                          // Handle web requests (5ms throttle)
  double level_dB = mic_handle_async();        // Get audio level (non-blocking queue)
  web_update_level(level_dB);                  // Update web interface
  led_handle_async(level_dB);                  // Update LEDs (manages decay/response timing)
  
  // Log every 5000 cycles to verify loop is running
  if (++loop_count % 5000 == 0) {
    log_i("[LOOP] alive: count=%lu dB=%.1f", loop_count, level_dB);
  }
  
  yield();  // Allow watchdog & FreeRTOS to handle tasks
}