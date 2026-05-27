#include <Arduino.h>
#include "microphone.h"
#include "config.h"
#include <driver/i2s.h>
#include <cmath>

//
// SOS IIR FILTER STRUCTURE
//
struct SOS_IIR_Filter {
  float gain;
  struct {
    float b1, b2, a1, a2;
  } sos[6];
};

// INMP441 Equalizer Filter
const SOS_IIR_Filter INMP441_filter = {
  .gain = 1.00197834654696,
  .sos = {{-1.986920458344451, +0.986963226946616, +1.995178510504166, -0.995184322194091}}
};

// A-weighting Filter
const SOS_IIR_Filter A_weighting_filter = {
  .gain = 0.169994948147430,
  .sos = {
    {-2.00026996133106, +1.00027056142719, -1.060868438509278, -0.163987445885926},
    {+4.35912384203144, +3.09120265783884, +1.208419926363593, -0.273166998428332},
    {+0.50612794482493, +0.04765969541352, +1.199801396247742, -0.322883302271233},
    {-0.70930303489759, -0.29071868393580, +1.982242159753048, -0.982298594928989}
  }
};

// Global filter state arrays
static float inmp441_state[1][2] = {0};  // 1 section, 2 state vars
static float aweight_state[4][2] = {0};  // 4 sections, 2 state vars each

// Global queue and buffer
QueueHandle_t samples_queue;
float samples[SAMPLES_SHORT] __attribute__((aligned(4)));  // I2S writes int32_t here, then converted to float

//
// FILTER IMPLEMENTATIONS
//

// Filter wrapper for INMP441 (uses dedicated state)
float filter_equalizer(float *input, float *output, size_t count) {
  float sum_sqr = 0;
  for (size_t i = 0; i < count; i++) {
    float sample = input[i];
    
    // Single SOS section
    float *state = inmp441_state[0];
    float w_n = sample - INMP441_filter.sos[0].a1 * state[0] - INMP441_filter.sos[0].a2 * state[1];
    sample = w_n + INMP441_filter.sos[0].b1 * state[0] + INMP441_filter.sos[0].b2 * state[1];
    state[1] = state[0];
    state[0] = w_n;
    
    output[i] = sample * INMP441_filter.gain;
    sum_sqr += output[i] * output[i];
  }
  return sum_sqr;
}

// Filter wrapper for A-weighting (uses dedicated state array)
float filter_aweight(float *input, float *output, size_t count) {
  float sum_sqr = 0;
  for (size_t i = 0; i < count; i++) {
    float sample = input[i];
    
    // Apply 4 cascaded SOS sections
    for (int s = 0; s < 4; s++) {
      float *state = aweight_state[s];
      float w_n = sample - A_weighting_filter.sos[s].a1 * state[0] - A_weighting_filter.sos[s].a2 * state[1];
      sample = w_n + A_weighting_filter.sos[s].b1 * state[0] + A_weighting_filter.sos[s].b2 * state[1];
      state[1] = state[0];
      state[0] = w_n;
    }
    
    output[i] = sample * A_weighting_filter.gain;
    sum_sqr += output[i] * output[i];
  }
  return sum_sqr;
}

//
// SOUND LEVEL PROCESSING
//

// Convert RMS amplitude to dB with safety checks
double mic_rms_to_db(float rms) {
  // Safety: prevent log10(0) and invalid math
  if (rms < 1e-8) {
    rms = 1e-8;
  }
  
  double db = MIC_OFFSET_DB + MIC_REF_DB + 20.0 * log10(rms / MIC_REF_AMPL);
  
  // Safety: clamp dB to reasonable range if calculation fails
  if (isnan(db) || isinf(db)) {
    db = 30.0;  // Default to quiet level
  }
  
  return db;
}

// Apply exponential smoothing to sound level
void mic_smooth_level(double &smoothed_db, double raw_db, float alpha) {
  smoothed_db = smoothed_db * (1.0 - alpha) + raw_db * alpha;
}

// Process audio samples from queue: RMS -> dB -> smoothing -> logging
double mic_handle_samples(const sum_queue_t &q) {
  static double smoothed_dB = 0;
  static unsigned long last_log = 0;
  
  if (q.sum_sqr_weighted <= 0) {
    return smoothed_dB;  // No valid data, return current value
  }
  
  // Calculate RMS from squared sum
  double rms = sqrt(q.sum_sqr_weighted / SAMPLES_SHORT);
  
  // Convert RMS to dB (with safety checks)
  double raw_dB = mic_rms_to_db(rms);
  
  // Apply exponential smoothing
  mic_smooth_level(smoothed_dB, raw_dB, 0.3);
  
  // Log measured level every 1 second
  unsigned long now = millis();
  if (now - last_log > 1000) {
    log_i("[SOUND] measured dB: %.1f", smoothed_dB);
    last_log = now;
  }
  
  return smoothed_dB;
}

// Async microphone handler (non-blocking queue check + processing)
double mic_handle_async() {
  static double current_level = 30.0;
  
  sum_queue_t q;
  // Non-blocking queue check (0ms timeout)
  if (xQueueReceive(samples_queue, &q, 0) == pdTRUE) {
    current_level = mic_handle_samples(q);
  }
  
  return current_level;
}

//
// SOS IIR FILTER IMPLEMENTATION
//
float filter_sos(const SOS_IIR_Filter *filt, float *input, float *output, size_t count, int num_sections) {
  float sum_sqr = 0;
  
  for (size_t i = 0; i < count; i++) {
    float sample = input[i];
    
    // Apply each cascaded SOS section
    for (int s = 0; s < num_sections; s++) {
      float *state = (s == 0) ? inmp441_state[0] : aweight_state[s];
      
      // Direct Form II: w(n) = x(n) - a1*w(n-1) - a2*w(n-2)
      float w_n = sample - filt->sos[s].a1 * state[0] - filt->sos[s].a2 * state[1];
      
      // y(n) = b1*w(n-1) + b2*w(n-2) (b0=1 incorporated in input)
      sample = w_n + filt->sos[s].b1 * state[0] + filt->sos[s].b2 * state[1];
      
      // Update state
      state[1] = state[0];
      state[0] = w_n;
    }
    
    // Apply gain
    sample *= filt->gain;
    output[i] = sample;
    sum_sqr += sample * sample;
  }
  
  return sum_sqr;
}

//
// I2S INITIALIZATION
//
esp_err_t mic_i2s_init() {
  log_i("I2S init: rate=%d bits=%d", SAMPLE_RATE, SAMPLE_BITS);
  
  // Initialize L/R select pin (HIGH for RIGHT channel)
  pinMode(I2S_LR, OUTPUT);
  digitalWrite(I2S_LR, HIGH);
  log_i("I2S L/R pin: GPIO%d set HIGH", I2S_LR);
  
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(SAMPLE_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = DMA_BANKS,
    .dma_buf_len = DMA_BANK_SIZE,
    .use_apll = true,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  esp_err_t install_err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (install_err != ESP_OK) {
    log_e("I2S install failed: %d", install_err);
    return install_err;
  }
  log_i("I2S driver installed");
  
  esp_err_t pin_err = i2s_set_pin(I2S_PORT, &pin_config);
  if (pin_err != ESP_OK) {
    log_e("I2S pin config failed: %d", pin_err);
    return pin_err;
  }
  log_i("I2S pins: WS=%d SCK=%d SD=%d", I2S_WS, I2S_SCK, I2S_SD);
  
  return ESP_OK;
}

//
// I2S READER TASK - REAL MICROPHONE
//
void mic_i2s_reader_task(void *parameter) {
  log_i("I2S Reader Task started");
  
  esp_err_t err = mic_i2s_init();
  if (err != ESP_OK) {
    log_e("I2S init failed: %d", err);
    vTaskDelete(NULL);
    return;
  }
  log_i("I2S init OK");

  // Discard first block
  size_t bytes_read = 0;
  err = i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, 150 / portTICK_PERIOD_MS);
  if (err != ESP_OK) {
    log_e("First I2S read failed: %d", err);
    vTaskDelete(NULL);
    return;
  }
  log_i("I2S reader ready");

  uint32_t sample_count = 0;
  while (true) {
    err = i2s_read(I2S_PORT, &samples, SAMPLES_SHORT * sizeof(int32_t), &bytes_read, 150 / portTICK_PERIOD_MS);
    if (err != ESP_OK) {
      log_e("I2S read failed: %d", err);
      continue;
    }

    // Convert samples to float
    SAMPLE_T *int_samples = (SAMPLE_T *)&samples;
    for (int i = 0; i < SAMPLES_SHORT; i++) {
      samples[i] = MIC_CONVERT(int_samples[i]);
    }
    
    sum_queue_t q;
    // Calculate raw RMS
    q.sum_sqr_SPL = 0;
    for (int i = 0; i < SAMPLES_SHORT; i++) {
      q.sum_sqr_SPL += samples[i] * samples[i];
    }
    
    // Debug: log first 5 raw int32 values on first block
    static bool first_block = true;
    if (first_block) {
      first_block = false;
      log_i("First 5 raw int32: %08x %08x %08x %08x %08x", 
        int_samples[0], int_samples[1], int_samples[2], int_samples[3], int_samples[4]);
    }
    
    q.sum_sqr_weighted = q.sum_sqr_SPL;
    q.proc_ticks = 0;

    xQueueSend(samples_queue, &q, portMAX_DELAY);
    
    if (++sample_count % 400 == 0) {
      log_d("I2S: %u blocks read", sample_count);
    }
  }
}

//
// MICROPHONE INITIALIZATION
//
void mic_init() {
  log_i("Queue: Creating...");
  samples_queue = xQueueCreate(8, sizeof(sum_queue_t));
  if (samples_queue == NULL) {
    log_e("ERROR: Queue creation failed!");
    while(1) delay(1000);
  }
  log_i("Queue: OK");

  log_i("Task: Creating I2S reader...");
  BaseType_t task_result = xTaskCreate(mic_i2s_reader_task, "Mic I2S Reader", I2S_TASK_STACK, NULL, I2S_TASK_PRI, NULL);
  if (task_result != pdPASS) {
    log_e("ERROR: I2S task creation failed!");
    while(1) delay(1000);
  }
  log_i("Task: OK");
}
