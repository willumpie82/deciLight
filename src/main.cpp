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

// Forward declarations for task functions
void webTask(void *param);
void ledTask(void *param);

// Global queue for passing dB level to LED task
QueueHandle_t dB_queue = NULL;


//
// WEB TASK - Handles HTTP requests and deferred config saves
//
void webTask(void *param) {
  log_i("[TASK] WebTask started on core %d", xPortGetCoreID());
  
  while (1) {
    // Handle incoming web requests (non-blocking)
    web_server.handleClient();
    
    // Check if config save is pending (deferred from HTTP handler)
    if (needs_save) {
      needs_save = false;
      web_save_config();
      log_i("Config updated via web: decay=%dms response=%dms",
        runtime_config.decay_ms,
        runtime_config.response_ms);
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));  // Yield for 50ms
  }
}


//
// LED TASK - Updates LED display based on sound level
//
void ledTask(void *param) {
  log_i("[TASK] LEDTask started on core %d", xPortGetCoreID());
  double level_dB = 0.0;
  
  while (1) {
    // Try to get latest dB value from queue (non-blocking, 0ms timeout)
    if (xQueueReceive(dB_queue, &level_dB, 0) == pdTRUE) {
      // Update web interface with current level
      web_update_level(level_dB);
      
      // Update LEDs with decay/response timing
      led_handle_async(level_dB);
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));  // Yield for 50ms
  }
}


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

  // Microphone setup (creates I2S reader task)
  mic_init();
  
  // Web interface setup (WiFi AP mode)
  web_init();
  
  // Create queue for dB values (size=1, just need latest value)
  dB_queue = xQueueCreate(1, sizeof(double));
  if (!dB_queue) {
    log_e("Failed to create dB_queue!");
    return;
  }
  
  // Create web server task (high priority, on core 0)
  BaseType_t web_ret = xTaskCreatePinnedToCore(
    webTask,           // Task function
    "WebTask",         // Task name
    4096,              // Stack size
    NULL,              // Parameter
    3,                 // Priority (higher = more important)
    NULL,              // Task handle
    0                  // Core (0 or 1, use core 0)
  );
  if (web_ret != pdPASS) {
    log_e("Failed to create WebTask!");
    return;
  }
  
  // Create LED task (medium priority, on core 1)
  BaseType_t led_ret = xTaskCreatePinnedToCore(
    ledTask,           // Task function
    "LEDTask",         // Task name
    4096,              // Stack size
    NULL,              // Parameter
    2,                 // Priority
    NULL,              // Task handle
    1                  // Core (use core 1 for parallel execution)
  );
  if (led_ret != pdPASS) {
    log_e("Failed to create LEDTask!");
    return;
  }
  
  log_e("=== SYSTEM READY ===\n");
}

//
// MAIN LOOP - Coordinator task
// Gets audio level from microphone and distributes to LED task
//
void loop() {
  static unsigned long loop_count = 0;
  
  // Get audio level from microphone (non-blocking queue)
  double level_dB = mic_handle_async();
  
  // Send to LED task via queue (overwrites if full)
  if (dB_queue) {
    xQueueOverwrite(dB_queue, &level_dB);
  }
  
  // Log every 5000 cycles to verify loop is running
  // if (++loop_count % 5000 == 0) {
  //   log_i("[LOOP] alive: count=%lu dB=%.1f", loop_count, level_dB);
  // }
  
  yield();  // Allow FreeRTOS scheduler to run
}