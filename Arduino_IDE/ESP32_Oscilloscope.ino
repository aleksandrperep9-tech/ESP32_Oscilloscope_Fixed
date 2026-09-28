#include <Arduino.h>
#include <driver/i2s.h>
#include <driver/adc.h>
#include <soc/syscon_reg.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "esp_adc_cal.h"
#include "filters.h"

//#define DEBUG_SERIAL
//#define DEBUG_BUFF
#define DELAY 1000

#define WIDTH  240
#define HEIGHT 280

#define ADC_CHANNEL   ADC1_CHANNEL_5
#define NUM_SAMPLES   1000
#define I2S_NUM         (0)
#define BUFF_SIZE 40000
#define B_MULT BUFF_SIZE/NUM_SAMPLES
#define BUTTON_Ok        32
#define BUTTON_Plus        15
#define BUTTON_Minus        35
#define BUTTON_Back        34

TFT_eSPI    tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);

esp_adc_cal_characteristics_t adc_chars;

TaskHandle_t task_menu;
TaskHandle_t task_adc;

float v_div = 825;
float s_div = 10;
float offset = 0;
float toffset = 0;
uint8_t current_filter = 1;

enum Option {
  None,
  Autoscale,
  Vdiv,
  Sdiv,
  Offset,
  TOffset,
  Filter,
  Stop,
  Mode,
  Single,
  Clear,
  Reset,
  Probe,
  UpdateF,
  Cursor1,
  Cursor2
};

int8_t volts_index = 0;
int8_t tscale_index = 0;
uint8_t opt = None;

bool menu = false;
bool info = true;
bool set_value = false;

float RATE = 1000.0f;

bool auto_scale = false;
bool full_pix = true;
bool stop = false;
bool stop_change = false;

uint16_t i2s_buff[BUFF_SIZE];

bool single_trigger = false;
bool data_trigger = false;

volatile bool updating_screen = false;
volatile bool new_data = false;
volatile bool menu_action = false;

uint8_t digital_wave_option = 0;

volatile uint8_t btnok = 0;
volatile uint8_t btnpl = 0;
volatile uint8_t btnmn = 0;
volatile uint8_t btnbk = 0;

void IRAM_ATTR btok() { btnok = 1; }
void IRAM_ATTR btplus() { btnpl = 1; }
void IRAM_ATTR btminus() { btnmn = 1; }
void IRAM_ATTR btback() { btnbk = 1; }

void setup() {
  Serial.begin(115200);

  configure_i2s(1000000);

  setup_screen();

  pinMode(BUTTON_Ok, INPUT);
  pinMode(BUTTON_Plus, INPUT);
  pinMode(BUTTON_Minus, INPUT);
  pinMode(BUTTON_Back, INPUT);
  attachInterrupt(BUTTON_Ok, btok, RISING);
  attachInterrupt(BUTTON_Plus, btplus, RISING);
  attachInterrupt(BUTTON_Minus, btminus, RISING);
  attachInterrupt(BUTTON_Back, btback, RISING);

  characterize_adc();

#ifdef DEBUG_BUF
  debug_buffer();
#endif

  xTaskCreatePinnedToCore(
    core0_task,
    "menu_handle",
    10000,
    NULL,
    0,
    &task_menu,
    0);

  xTaskCreatePinnedToCore(
    core1_task,
    "adc_handle",
    10000,
    NULL,
    3,
    &task_adc,
    1);
}

void core0_task(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
    menu_handler();

    if (new_data || menu_action) {
      new_data = false;
      menu_action = false;

      updating_screen = true;
      update_screen(i2s_buff, RATE);
      updating_screen = false;

      vTaskDelay(pdMS_TO_TICKS(10));
      Serial.println("CORE0");
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void core1_task(void *pvParameters) {
  (void)pvParameters;

  for (;;) {
    if (!single_trigger) {
      while (updating_screen) {
        vTaskDelay(pdMS_TO_TICKS(1));
      }

      if (!stop) {
        if (stop_change) {
          i2s_adc_enable(I2S_NUM_0);
          stop_change = false;
        }
        ADC_Sampling(i2s_buff);
        new_data = true;
      } else {
        if (!stop_change) {
          i2s_adc_disable(I2S_NUM_0);
          i2s_zero_dma_buffer(I2S_NUM_0);
          stop_change = true;
        }
      }

      Serial.println("CORE1");
      vTaskDelay(pdMS_TO_TICKS(300));
    } else {
      float old_mean = 0.0f;
      while (single_trigger) {
        stop = true;
        ADC_Sampling(i2s_buff);

        float mean = 0.0f;
        float max_v, min_v;
        peak_mean(i2s_buff, BUFF_SIZE, &max_v, &min_v, &mean);

        if ((old_mean != 0.0f && fabs(mean - old_mean) > 0.2f) || (to_voltage(max_v) - to_voltage(min_v) > 0.05f)) {
          float freq = 0.0f;
          float period = 0.0f;
          uint32_t trigger0 = 0;
          uint32_t trigger1 = 0;

          bool digital_data = false;

          if (digital_wave_option == 1) {
            trigger_freq_analog(i2s_buff, RATE, mean, max_v, min_v, &freq, &period, &trigger0, &trigger1);
          } else if (digital_wave_option == 0) {
            digital_data = digital_analog(i2s_buff, max_v, min_v);
            if (!digital_data) {
              trigger_freq_analog(i2s_buff, RATE, mean, max_v, min_v, &freq, &period, &trigger0, &trigger1);
            } else {
              trigger_freq_digital(i2s_buff, RATE, mean, max_v, min_v, &freq, &period, &trigger0);
            }
          } else {
            trigger_freq_digital(i2s_buff, RATE, mean, max_v, min_v, &freq, &period, &trigger0);
          }

          single_trigger = false;
          new_data = true;
          Serial.println("Single GOT");
        }

        old_mean = mean;
        vTaskDelay(pdMS_TO_TICKS(1));
      }
      vTaskDelay(pdMS_TO_TICKS(300));
    }
  }
}

void loop() {}
