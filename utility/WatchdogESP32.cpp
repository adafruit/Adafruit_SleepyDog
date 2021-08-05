#if defined(ARDUINO_ARCH_ESP32)

#include "WatchdogESP32.h"
#include <esp_task_wdt.h>

int WatchdogESP32::enable(int maxPeriodMS) {
  if (maxPeriodMS < 0)
    return 0;

  // Enable the TWDT
  // and execute the esp32 panic handler when TWDT times out
  esp_task_wdt_init((uint32_t)maxPeriodMS, true);

  // NULL to subscribe the current running task to the TWDT
  esp_task_wdt_add(NULL);

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

// not implemented
void WatchdogESP32::sleep() {}

#endif // ARDUINO_ARCH_ESP32