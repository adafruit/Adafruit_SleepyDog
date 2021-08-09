#if defined(ARDUINO_ARCH_ESP32)

#include "WatchdogESP32.h"
#include "esp_task_wdt.h"
#include "esp_sleep.h"

int WatchdogESP32::enable(int maxPeriodMS) {
  if (maxPeriodMS < 0)
    return 0;

  // ESP32 expects TWDT in seconds
  uint32_t maxPeriod = maxPeriodMS / 1000;
  // Enable the TWDT and execute the esp32 panic handler when TWDT times out
  esp_err_t = esp_task_wdt_init(maxPeriod, true);
  if (err != ESP_OK)
    return 0; // Initialization failed due to lack of memory

  // NULL to subscribe the current running task to the TWDT
  err = esp_task_wdt_add(NULL);
  if (err != ESP_OK)
    return 0;

  _wdto = maxPeriodMS;
  return maxPeriodMS;
}

void WatchdogESP32::reset() {
  // Reset the Task Watchdog Timer (TWDT) on behalf
  // of the currently running task.
  // NOTE: This blindly resets the TWDT and does not return the esp_err.
  esp_task_wdt_reset();
}

void WatchdogESP32::disable() {
  // Unsubscribes the current running task from the TWDT
  esp_task_wdt_delete(NULL);
}

int WatchdogESP32::sleep(int maxPeriodMS) {
  if (maxPeriodMS < 0)
      return 0;
  // Convert from MS to microseconds
  uint64_t sleepTime = maxPeriodMS * 1000;
  // Enable wakeup by timer
  esp_err_t err = esp_sleep_enable_timer_wakeup(sleepTime);
  if (err != ESP_OK) {
      return 0; // sleepTime is out of range
  }
  // Enter light sleep with the timer wakeup option configured
  err = esp_light_sleep_start();
  if (err != ESP_OK) {
    return 0; // ESP_ERR_INVALID_STATE if WiFi or BT is not stopped
  }
  return maxPeriodMS;
}

#endif // ARDUINO_ARCH_ESP32