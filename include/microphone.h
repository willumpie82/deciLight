#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config.h"

//
// Microphone Data Structure
//
struct AudioSample {
  float sum_sqr_SPL;
  float sum_sqr_weighted;
  uint32_t proc_ticks;
};

//
// Microphone Class - Manages I2S microphone and audio processing
//
class Microphone {
public:
  Microphone();
  void init();              // Initialize I2S peripheral and reader task
  double getLevel();        // Get current smoothed dB level
  
private:
  static void i2sReaderTaskWrapper(void *param);  // Static task wrapper
  void i2sReaderTask();     // Instance I2S reader task

  // Audio processing methods
  double rmsToDb(float rms);
  void smoothLevel(double &smoothed_db, double raw_db, float alpha);
  double processSamples(const AudioSample &q);
  esp_err_t i2sInit();

  // Member variables
  TaskHandle_t reader_task_handle;
  double current_level;
  double smoothed_level;
  
  // Filter state arrays (members now)
  float inmp441_state[1][2];   // 1 section, 2 state vars
  float aweight_state[4][2];   // 4 sections, 2 state vars each
};

// Global instance
extern Microphone microphone;

// Global buffers for I2S reader task
extern float samples[SAMPLES_SHORT];
extern QueueHandle_t samples_queue;

#endif // MICROPHONE_H
