/*
 * Second-Order Sections (SOS) IIR Filter Implementation
 * 
 * Adapted from deciLight project
 * https://github.com/bbbenji/deciLight
 * 
 * Implements cascaded second-order sections for efficient DSP filtering
 */

#ifndef SOS_IIR_FILTER_H
#define SOS_IIR_FILTER_H

#include <cmath>
#include <algorithm>

/*
 * SOS_IIR_Filter
 * 
 * Represents a filter as Second-Order Sections with the form:
 *   H(z) = gain * ∏(1 + b1*z^-1 + b2*z^-2) / (1 - a1*z^-1 - a2*z^-2)
 * 
 * Each section is represented as {b1, b2, -a1, -a2}
 * assuming b0=1 and a0=1 (normalized)
 */

struct SOS_IIR_Filter {
  float gain;
  struct {
    float b1, b2, a1, a2;
  } sos[6];  // Up to 6 second-order sections
};

/*
 * Apply SOS IIR filter to samples
 * 
 * @param input Pointer to input samples (float array)
 * @param output Pointer to output buffer (can be same as input)
 * @param sample_count Number of samples to process
 * @return Sum of squared output samples
 */
static inline float apply_sos_iir_filter(const SOS_IIR_Filter *filter,
                                         float *input,
                                         float *output,
                                         size_t sample_count,
                                         int section_count) {
  float sum_sqr = 0;
  
  // Process each sample
  for (size_t i = 0; i < sample_count; i++) {
    float sample = input[i];
    
    // Apply each SOS section
    for (int s = 0; s < section_count; s++) {
      static float w[6][2] = {0};  // State variables for each section
      
      // Direct Form II implementation
      // w(n) = x(n) - a1*w(n-1) - a2*w(n-2)
      float w_n = sample - filter->sos[s].a1 * w[s][0] - filter->sos[s].a2 * w[s][1];
      
      // y(n) = b0*w(n) + b1*w(n-1) + b2*w(n-2)  (b0=1)
      sample = w_n + filter->sos[s].b1 * w[s][0] + filter->sos[s].b2 * w[s][1];
      
      // Update state
      w[s][1] = w[s][0];
      w[s][0] = w_n;
    }
    
    // Apply gain
    sample *= filter->gain;
    
    output[i] = sample;
    sum_sqr += sample * sample;
  }
  
  return sum_sqr;
}

/*
 * Class wrapper for SOS IIR Filter with state management
 */
class SOS_IIR_Filter_Cpp {
private:
  const SOS_IIR_Filter *filter_;
  int section_count_;
  float state_[6][2];  // State for each section: {w[n-1], w[n-2]}
  
public:
  SOS_IIR_Filter_Cpp(const SOS_IIR_Filter *filter, int section_count)
    : filter_(filter), section_count_(section_count) {
    // Initialize state
    for (int s = 0; s < 6; s++) {
      state_[s][0] = 0;
      state_[s][1] = 0;
    }
  }
  
  // Process samples and return sum of squares
  float filter(float *input, float *output, size_t sample_count) {
    float sum_sqr = 0;
    
    for (size_t i = 0; i < sample_count; i++) {
      float sample = input[i];
      
      // Apply cascaded SOS sections
      for (int s = 0; s < section_count_; s++) {
        // Direct Form II: w(n) = x(n) - a1*w(n-1) - a2*w(n-2)
        float w_n = sample - filter_->sos[s].a1 * state_[s][0] 
                           - filter_->sos[s].a2 * state_[s][1];
        
        // y(n) = b1*w(n-1) + b2*w(n-2) (b0=1 incorporated above)
        sample = w_n + filter_->sos[s].b1 * state_[s][0] 
                     + filter_->sos[s].b2 * state_[s][1];
        
        // Update state
        state_[s][1] = state_[s][0];
        state_[s][0] = w_n;
      }
      
      // Apply gain at the end
      sample *= filter_->gain;
      
      output[i] = sample;
      sum_sqr += sample * sample;
    }
    
    return sum_sqr;
  }
};

#endif // SOS_IIR_FILTER_H
