void peak_mean(uint16_t *i2s_buffer, uint32_t len, float *max_value, float *min_value, float *pt_mean) {
  if (!i2s_buffer || !max_value || !min_value || !pt_mean || len == 0) {
    return;
  }

  max_value[0] = i2s_buffer[0];
  min_value[0] = i2s_buffer[0];

  mean_filter filter(5);
  filter.init(i2s_buffer[0]);

  float mean = 0.0f;
  for (uint32_t i = 1; i < len; i++) {
    float value = filter.filter((float)i2s_buffer[i]);

    if (value > max_value[0]) {
      max_value[0] = value;
    }
    if (value < min_value[0]) {
      min_value[0] = value;
    }

    mean += (float)i2s_buffer[i];
  }

  mean /= (float)len;
  pt_mean[0] = to_voltage(mean);
}

bool digital_analog(uint16_t *i2s_buffer, uint32_t max_v, uint32_t min_v) {
  if (!i2s_buffer) {
    return false;
  }

  uint32_t range = max_v - min_v;
  if (range == 0) {
    return false;
  }

  uint32_t upper_threshold = max_v - (uint32_t)(0.05f * range);
  uint32_t lower_threshold = min_v + (uint32_t)(0.05f * range);

  uint32_t digital_data = 0;
  uint32_t analog_data = 0;

  for (uint32_t i = 0; i < BUFF_SIZE; i++) {
    if (i2s_buffer[i] > lower_threshold) {
      if (i2s_buffer[i] > upper_threshold) {
        digital_data++;
      } else {
        analog_data++;
      }
    } else {
      digital_data++;
    }
  }

  return (analog_data < digital_data);
}

void trigger_freq_analog(uint16_t *i2s_buffer,
                         float sample_rate,
                         float mean,
                         uint32_t max_v,
                         uint32_t min_v,
                         float *pt_freq,
                         float *pt_period,
                         uint32_t *pt_trigger0,
                         uint32_t *pt_trigger1) {
  float freq = 0.0f;
  float period = 0.0f;
  bool signal_side = false;
  uint32_t trigger_count = 0;
  uint32_t trigger_num = 10;
  uint32_t trigger_temp[trigger_num] = {0};
  uint32_t trigger_index = 0;

  if (to_voltage(i2s_buffer[0]) > mean) {
    signal_side = true;
  }

  uint32_t wave_center = (max_v + min_v) / 2;
  for (uint32_t i = 1; i < BUFF_SIZE; i++) {
    if (signal_side && i2s_buffer[i] < wave_center - (wave_center - min_v) * 0.2f) {
      signal_side = false;
    } else if (!signal_side && i2s_buffer[i] > wave_center + (max_v - wave_center) * 0.2f) {
      freq++;
      if (trigger_count < trigger_num) {
        trigger_temp[trigger_count] = i;
        trigger_count++;
      }
      signal_side = true;
    }
  }

  if (trigger_count < 2) {
    trigger_temp[0] = 0;
    trigger_index = 0;
    freq = 0.0f;
    period = 0.0f;
  } else {
    freq = freq * 1000.0f / 50.0f;
    if (freq <= 0.0f) {
      freq = 0.0f;
      period = 0.0f;
    } else {
      period = (float)(sample_rate * 1000.0f) / freq;

      if (freq < 2000.0f && freq > 80.0f) {
        period = 0.0f;
        for (uint32_t i = 1; i < trigger_count; i++) {
          period += trigger_temp[i] - trigger_temp[i - 1];
        }
        period /= (trigger_count - 1);
        freq = sample_rate * 1000.0f / period;
      } else if (freq <= 80.0f) {
        period = trigger_temp[1] - trigger_temp[0];
        freq = sample_rate * 1000.0f / period;
      }
    }
  }

  uint32_t trigger2 = 0;
  if (trigger_count > 1) {
    if (trigger_temp[0] - (uint32_t)(period * 0.05f) > 0) {
      trigger_index = trigger_temp[0] - (uint32_t)(period * 0.05f);
      trigger2 = trigger_temp[1] - (uint32_t)(period * 0.05f);
    } else if (trigger_count > 2) {
      trigger_index = trigger_temp[1] - (uint32_t)(period * 0.05f);
      trigger2 = trigger_temp[2] - (uint32_t)(period * 0.05f);
    }
  }

  pt_trigger0[0] = trigger_index;
  pt_trigger1[0] = trigger2;
  pt_freq[0] = freq;
  pt_period[0] = period;
}

void trigger_freq_digital(uint16_t *i2s_buffer,
                          float sample_rate,
                          float mean,
                          uint32_t max_v,
                          uint32_t min_v,
                          float *pt_freq,
                          float *pt_period,
                          uint32_t *pt_trigger0) {

  float freq = 0.0f;
  float period = 0.0f;
  bool signal_side = false;
  uint32_t trigger_count = 0;
  uint32_t trigger_num = 10;
  uint32_t trigger_temp[trigger_num] = {0};
  uint32_t trigger_index = 0;

  if (to_voltage(i2s_buffer[0]) > mean) {
    signal_side = true;
  }

  uint32_t wave_center = (max_v + min_v) / 2;
  bool normal_high = (mean > to_voltage(wave_center)) ? true : false;

  if (max_v - min_v > 4095u * (0.4f / 3.3f)) {
    for (uint32_t i = 1; i < BUFF_SIZE; i++) {
      if (signal_side && i2s_buffer[i] < wave_center - (wave_center - min_v) * 0.2f) {
        if (trigger_count < trigger_num && normal_high) {
          trigger_temp[trigger_count] = i;
          trigger_count++;
        }
        signal_side = false;
      } else if (!signal_side && i2s_buffer[i] > wave_center + (max_v - wave_center) * 0.2f) {
        freq++;
        if (trigger_count < trigger_num && !normal_high) {
          trigger_temp[trigger_count] = i;
          trigger_count++;
        }
        signal_side = true;
      }
    }

    freq = freq * 1000.0f / 50.0f;

    if (freq <= 0.0f) {
      freq = 0.0f;
      period = 0.0f;
    } else {
      period = (float)(sample_rate * 1000.0f) / freq;

      if (trigger_count > 1) {
        if (freq < 2000.0f && freq > 80.0f) {
          period = 0.0f;
          for (uint32_t i = 1; i < trigger_count; i++) {
            period += trigger_temp[i] - trigger_temp[i - 1];
          }
          period /= (trigger_count - 1);
          freq = sample_rate * 1000.0f / period;
        } else if (freq <= 80.0f) {
          period = trigger_temp[1] - trigger_temp[0];
          freq = sample_rate * 1000.0f / period;
        }
      }
    }

    trigger_index = trigger_temp[0];
    if (trigger_index > 10) {
      trigger_index -= 10;
    } else {
      trigger_index = 0;
    }
  }

  pt_trigger0[0] = trigger_index;
  pt_freq[0] = freq;
  pt_period[0] = period;
}
