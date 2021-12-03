#if defined(ARDUINO_ARCH_ESP8266)

#include "WatchdogESP8266.h"

/**************************************************************************/
/*!
    @brief  Initializes the ESP8266's software WDT
    @param    maxPeriodMS
              Timeout period of WDT in milliseconds
    @return The actual period (in milliseconds) before a watchdog timer
            reset is returned. 0 otherwise.
*/
/**************************************************************************/
int WatchdogESP8266::enable(int maxPeriodMS) {
  ESP.wdtDisable();
  if (maxPeriodMS < 0)
    return 0;

  // Enable the WDT
  ESP.wdtEnable((uint32_t)maxPeriodMS);

  _wdto = maxPeriodMS;
  return maxPeriodMS;
}

/**************************************************************************/
/*!
    @brief  Feeds the Watchdog Timer.
*/
/**************************************************************************/
void WatchdogESP8266::reset() { ESP.wdtFeed(); }

/**************************************************************************/
/*!
    @brief  Disables the Watchdog Timer.
        NOTE: Please don't stop software watchdog too long
        (less than 6 seconds), otherwise it will trigger the hardware
        watchdog reset.
*/
/**************************************************************************/
void WatchdogESP8266::disable() { ESP.wdtDisable(); }

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
int WatchdogESP8266::sleep(int maxPeriodMS) {
  if (maxPeriodMS < 0)
    return 0;
  // Convert from MS to microseconds
  uint64_t sleepTime = maxPeriodMS * 1000;

  // Assert that we can not sleep longer than the max. time calculated by ESP
  if (sleepTime > ESP.deepSleepMax())
    return 0;

  // Enters deep sleep with mode WAKE_RF_DEFAULT
  ESP.deepSleep(sleepTime);

  return maxPeriodMS;
}

#endif // ARDUINO_ARCH_ESP8266