#ifndef MICROPHONE_H
#define MICROPHONE_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "config.h"

//
// QUEUES & BUFFERS
//
struct sum_queue_t {
  float sum_sqr_SPL;
  float sum_sqr_weighted;
  uint32_t proc_ticks;
};

extern QueueHandle_t samples_queue;
extern float samples[SAMPLES_SHORT];

// Microphone functions
void mic_init();
esp_err_t mic_i2s_init();
void mic_i2s_reader_task(void *parameter);

// Sound level processing functions
double mic_rms_to_db(float rms);
void mic_smooth_level(double &smoothed_db, double raw_db, float alpha);
double mic_handle_samples(const sum_queue_t &q);

// Async handler (fetches queue data and processes)
double mic_handle_async();

#endif // MICROPHONE_H
