#if defined(ARDUINO_ARCH_ESP32)

#include "WatchdogESP32.h"

/**************************************************************************/
/*!
    @brief  Initializes the ESP32's Task Watchdog Timer (TWDT) and
            subscribes to the current running task.
    @param    maxPeriodMS
              Timeout period of TWDT in seconds
    @return The actual period (in milliseconds) before a watchdog timer
            reset is returned. 0 otherwise.
*/
/**************************************************************************/
int WatchdogESP32::enable(int maxPeriodMS) {
  if (maxPeriodMS < 0)
    return 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 1)
// Initialize the wdt configuration for ESP-IDF v5.x and above
#include "soc/soc_caps.h"
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = (uint32_t)maxPeriodMS,
      .idle_core_mask = (1 << SOC_CPU_CORES_NUM) - 1, // Bitmask of all cores
      .trigger_panic = true,
  };
  esp_err_t err = esp_task_wdt_init(&wdt_config);
  // Reconfigure in case TWDT was already initialized
  if (err == ESP_ERR_INVALID_STATE)
    err = esp_task_wdt_reconfigure(&wdt_config);
#else
  // IDF V4.x and below expect TWDT in seconds
  uint32_t maxPeriod = maxPeriodMS / 1000;
  // Enable the TWDT and execute the esp32 panic handler when TWDT times out
  esp_err_t err = esp_task_wdt_init(maxPeriod, true);
#endif

  if (err != ESP_OK)
    return 0; // Failed to initialize TWDT

  // NULL to subscribe the current running task to the TWDT
  err = esp_task_wdt_add(NULL);
  if (err != ESP_OK)
    return 0; // Failed to subscribe to TWDT, may be already subscribed

  _wdto = maxPeriodMS;
  return maxPeriodMS;
}

/**************************************************************************/
/*!
    @brief  Resets the Task Watchdog Timer (TWDT) on behalf
            of the currently running task.
*/
/**************************************************************************/
void WatchdogESP32::reset() {
  // NOTE: This blindly resets the TWDT and does not return the esp_err.
  esp_task_wdt_reset();
}

/**************************************************************************/
/*!
    @brief  Unsubscribes the currently running task from the the TWDT,
            unsubscribes idle tasks from the TWDT, and deinitializes
            the TWDT.
*/
/**************************************************************************/
void WatchdogESP32::disable() {
  esp_task_wdt_delete(NULL); // Unsubscribe from the task we created
  esp_task_wdt_deinit(); // Deinitialize the TWDT and unsubscribe from any idle
                         // tasks
  _wdto = 0;             // Reset the timeout value
}

/**************************************************************************/
/*!
    @brief  Configures the ESP32 to enter a low-power sleep mode for a
            desired amount of time.
    @param    maxPeriodMS
              Time to sleep the ESP32, in millis.
    @return The actual period (in milliseconds) that the hardware was
            asleep will be returned. Otherwise, 0 will be returned if the
            hardware could not enter the low-power mode.
*/
/**************************************************************************/
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