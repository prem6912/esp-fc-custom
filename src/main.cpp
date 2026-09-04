#include <Arduino.h>
#ifdef ESP32
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "esp_task_wdt.h"

// Disable brownout detector at the absolute earliest point in C-runtime startup
__attribute__((constructor(101))) static void disable_early_brownout(void)
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
}
#endif



#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Espfc.h>
#include <Kalman.h>
#include <Madgwick.h>
#include <Mahony.h>
#include <printf.h>
#include <blackbox/blackbox.h>
#include <EscDriver.h>
#include <EspWire.h>
#include <Gps.hpp>
#if defined(ESPFC_ESPNOW)
#include <EspNowRcLink/Receiver.h>
#endif
#ifdef ESPFC_WIFI_ALT
#include <ESP8266WiFi.h>
#elif defined(ESPFC_WIFI)
#include <WiFi.h>
#endif
#include "Debug_Espfc.h"

#ifdef ESP32
void IRAM_ATTR serialEventRun(void) {}
#endif

Espfc::Espfc espfc;

#if defined(ESPFC_MULTI_CORE)
  #if defined(ESPFC_FREE_RTOS)

    // ESP32 multicore
    #include <freertos/FreeRTOS.h>
    #include <freertos/task.h>
    #include <driver/timer.h>

    TaskHandle_t gyroTaskHandle = NULL;
    TaskHandle_t pidTaskHandle = NULL;
    static const timer_group_t TIMER_GROUP = TIMER_GROUP_0;
    static const timer_idx_t TIMER_IDX = TIMER_0;

    bool IRAM_ATTR gyroTimerIsr(void* args)
    {
      BaseType_t xHigherPriorityTaskWoken;
      vTaskNotifyGiveFromISR(gyroTaskHandle, &xHigherPriorityTaskWoken);
      return xHigherPriorityTaskWoken == pdTRUE;
    }

    void gyroTimerInit(bool (*isrCb)(void* args), int interval)
    {
      timer_config_t config = {
        .alarm_en = TIMER_ALARM_EN,
        .counter_en = TIMER_PAUSE,
        .intr_type = TIMER_INTR_LEVEL,
        .counter_dir = TIMER_COUNT_UP,
        .auto_reload = TIMER_AUTORELOAD_EN,
        .divider = 80,
      };
      timer_init(TIMER_GROUP, TIMER_IDX, &config);
      timer_set_counter_value(TIMER_GROUP, TIMER_IDX, 0);
      timer_set_alarm_value(TIMER_GROUP, TIMER_IDX, interval);
      timer_isr_callback_add(TIMER_GROUP, TIMER_IDX, isrCb, nullptr, ESP_INTR_FLAG_IRAM);
      timer_enable_intr(TIMER_GROUP, TIMER_IDX);
      timer_start(TIMER_GROUP, TIMER_IDX);
    }

    void gyroTask(void *pvParameters)
    {
      espfc.begin();
      gyroTimerInit(gyroTimerIsr, espfc.getGyroInterval());
      while(true)
      {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // wait for timer isr notification
        espfc.update(true);
      }
    }

    void pidTask(void *pvParameters)
    {
      disableCore0WDT();
      TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
      TIMERG0.wdt_config0.en = 0;
      TIMERG0.wdt_wprotect = 0;
      while(true)
      {
        espfc.updateOther();
        vTaskDelay(1);
      }
    }

    void setup()
    {
      // 1. Fully disable Hardware Brownout Detector
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

      // 2. Disable Hardware Watchdogs
      disableCore0WDT();
      TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
      TIMERG0.wdt_config0.en = 0;
      TIMERG0.wdt_wprotect = 0;

      TIMERG1.wdt_wprotect = TIMG_WDT_WKEY_VALUE;
      TIMERG1.wdt_config0.en = 0;
      TIMERG1.wdt_wprotect = 0;

      espfc.load();
      xTaskCreateUniversal(gyroTask, "gyroTask", 8192, NULL, 24, &gyroTaskHandle, 1);
      xTaskCreateUniversal(pidTask,  "pidTask",  8192, NULL,  1, &pidTaskHandle,  0);

      vTaskDelete(NULL); // delete arduino loop task
    }

    void loop()
    {
    }

  #elif defined(ESPFC_MULTI_CORE_RP2040)

    bool core1_separate_stack = true;
    volatile bool setup_done = false;

    // RP2040 multicore
    // TODO: https://emalliab.wordpress.com/2021/04/18/raspberry-pi-pico-arduino-core-and-timers/
    void setup()
    {
      espfc.load();
      espfc.begin();
      setup_done = true;
    }
    void loop()
    {
      espfc.update();
    }
    void setup1()
    {
      while(!setup_done);
    }
    void loop1()
    {
      espfc.updateOther();
    }

  #else
    #error "No RTOS defined for multicore board"
  #endif

#else

  // single core
  void setup()
  {
    espfc.load();
    espfc.begin();
  }
  void loop()
  {
    espfc.update();
    espfc.updateOther();
  }

#endif
