void configure_i2s(int rate) {
  i2s_config_t i2s_config =
  {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = rate,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ALL_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 1,
    .dma_buf_count = 2,
    .dma_buf_len = NUM_SAMPLES,
    .use_apll = 0,
  };

  adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);
  adc1_config_width(ADC_WIDTH_BIT_12);

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_adc_mode(ADC_UNIT_1, ADC_CHANNEL);
  SET_PERI_REG_MASK(SYSCON_SARADC_CTRL2_REG, SYSCON_SARADC_SAR1_INV);
  i2s_adc_enable(I2S_NUM_0);
}

void ADC_Sampling(uint16_t *i2s_buff) {
  size_t bytes_read = 0;
  for (int i = 0; i < B_MULT; i++) {
    i2s_read(I2S_NUM_0, (void*)&i2s_buff[i * NUM_SAMPLES], NUM_SAMPLES * sizeof(uint16_t), &bytes_read, portMAX_DELAY);
  }
}

void set_sample_rate(uint32_t rate) {
  i2s_driver_uninstall(I2S_NUM_0);
  configure_i2s(rate);
}
