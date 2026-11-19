#if defined(ARDUINO_ARCH_RP2040)

#include "WatchdogRP2040.h"
#include "SleepyDogRP.h"

/**********************************************************************************************/
/*!
    @brief  Initializes the RP2040's hardware watchdog timer.
    @param    maxPeriodMS
              Timeout period of WDT in milliseconds
    @return The actual period (in milliseconds) before a watchdog timer
            reset is returned, 0 otherwise.
*/
/**********************************************************************************************/
int WatchdogRP2040::enable(int maxPeriodMS) {
  if (maxPeriodMS < 0)
    return 0;

  // Enables the RP2040's hardware WDT with maxPeriodMS delay
  // (wdt should be updated every maxPeriodMS ms) and
  // enables pausing the WDT on debugging when stepping thru
  watchdog_enable(maxPeriodMS, 1);

  _wdto = maxPeriodMS;
  // remembered so a sleep can re-arm the WDT with the same timeout
  sleepydog_rp_wdt_note(maxPeriodMS);
  return maxPeriodMS;
}

/**************************************************************************/
/*!
    @brief  Reload the watchdog counter with the amount of time set in
            enable().
*/
/**************************************************************************/
void WatchdogRP2040::reset() { watchdog_update(); }

/**************************************************************************/
/*!
    @brief  Disables the RP2040's hardware watchdog timer (clears
            WATCHDOG_CTRL.ENABLE, the same as the SDK's watchdog_disable()).
*/
/**************************************************************************/
void WatchdogRP2040::disable() {
  hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
  _wdto = -1;
  sleepydog_rp_wdt_note(0);
}

/**************************************************************************/
/*!
    @brief  Puts the RP2040 into light sleep for a period of time.

            With arduino-pico >= 6.4.0 this is a real low-power sleep
            (LowPower.sleepFor(): clocks gated, PLLs stopped, the sketch
            resumes where it stopped, RAM and pins kept). A watchdog
            enabled with enable() is paused for the sleep and re-armed with
            the same timeout afterwards. On older cores, with FreeRTOS or in
            TinyUSB host-mode builds it falls back to sleep_ms().
    @param    maxPeriodMS
              Time to sleep the RP2040, in millis. 0 returns at once.
    @return The period (in milliseconds) that the hardware was asleep, 0 if
            it did not sleep.
*/
/**************************************************************************/
int WatchdogRP2040::sleep(int maxPeriodMS) {
  return sleepydog_rp_watchdog_sleep(maxPeriodMS);
}

#endif // ARDUINO_ARCH_RP2040
