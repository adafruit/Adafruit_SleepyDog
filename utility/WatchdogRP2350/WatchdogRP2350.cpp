#if defined(ARDUINO_ARCH_RP2350) || defined(PICO_RP2350) ||                    \
    defined(TARGET_RP2350)

#include "WatchdogRP2350.h"

/**********************************************************************************************/
/*!
    @brief  Initializes the RP2350's hardware watchdog timer.
    @param    maxPeriodMS
              Timeout period of WDT in milliseconds
    @return The actual period (in milliseconds) before a watchdog timer
            reset is returned, 0 otherwise.
*/
/**********************************************************************************************/
int WatchdogRP2350::enable(int maxPeriodMS) {
  if (maxPeriodMS < 0)
    return 0;

  // Enables the RP2350's hardware WDT with maxPeriodMS delay
  // (wdt should be updated every maxPeriodMS ms) and
  // enables pausing the WDT on debugging when stepping thru
  watchdog_enable(maxPeriodMS, 1);

  _wdto = maxPeriodMS;
  return maxPeriodMS;
}

/**************************************************************************/
/*!
    @brief  Reload the watchdog counter with the amount of time set in
            enable().
*/
/**************************************************************************/
void WatchdogRP2350::reset() { watchdog_update(); }

/**************************************************************************/
/*!
    @brief  Once enabled, the RP2350's Watchdog Timer can NOT be disabled.
*/
/**************************************************************************/
void WatchdogRP2350::disable() {}

/**************************************************************************/
/*!
    @brief  Configures the RP2350 to enter a lower power (WFE) sleep
            for a period of time.
    @param    maxPeriodMS
              Time to sleep the RP2350, in millis.
    @return The actual period (in milliseconds) that the hardware was
            asleep will be returned. Otherwise, 0 will be returned if the
            hardware could not enter the low-power mode.
*/
/**************************************************************************/
int WatchdogRP2350::sleep(int maxPeriodMS) {
  if (maxPeriodMS < 0)
    return 0;

  // perform a lower power (WFE) sleep (pico-core calls sleep_ms(sleepTime))
  sleep_ms(maxPeriodMS);

  return maxPeriodMS;
}

/**************************************************************************/
/*!
    @brief  Re-enables clock sources and generators after waking from sleep,
   must be called by user code.
*/
/**************************************************************************/
void WatchdogRP2350::resumeFromSleep() {
  // Re-enable clock sources and generators
  sleep_power_up();

// Try to reattach USB connection on "native USB" boards (connection is
// lost on sleep). Host will also need to reattach to the Serial monitor.
// Seems not entirely reliable, hence the LED indicator fallback.
#if defined(USBCON) && !defined(USE_TINYUSB)
  USBDevice.attach();
#endif
}

/**************************************************************************/
/*!
    @brief  Sets a callback function to be called upon wake from sleep. Only one
   callback can be set at a time.
    @param  cb
            Pointer to the user-defined callback function.
*/
/**************************************************************************/
void WatchdogRP2350::setWakeCb(WakeCb cb) { _cb_wake = cb; }

/**************************************************************************/
/*!
    @brief  Gets the duration of the previous sleep cycle, in milliseconds.
    @return The sleep duration in milliseconds.
*/
/**************************************************************************/
long WatchdogRP2350::getSleepDuration() {
  // Obtain the current time from the AON timer
  struct timespec ts_sleep_end;
  aon_timer_get_time(&ts_sleep_end);
  // Compare timespecs to get sleep duration
  long sleep_duration_ms =
      (ts_sleep_end.tv_sec - _ts_sleep_start.tv_sec) * 1000 +
      (ts_sleep_end.tv_nsec - _ts_sleep_start.tv_nsec) / 1000000;
  return sleep_duration_ms;
}

/**************************************************************************/
/*!
    @brief  Puts the RP2350 into Sleep State (6.5.2, 6.5.3 in RP2350 Datasheet)
   for a specified period of time, uses the AON timer to wake the device.
    @param  max_period_ms
            Desired sleep period, in milliseconds.
    @param  is_dormant
            If true, enters Dormant State (6.5.3) instead of Sleep State.
*/
/**************************************************************************/
void WatchdogRP2350::goToSleepUntil(int max_period_ms, bool is_dormant) {
  if (max_period_ms < 0)
    return;

  // Configure the AON timer
  startAonTimer();

  // Get and store the sleep start time
  aon_timer_get_time(&_ts_sleep_start);

  // Configure the sleep clock source
  if (!is_dormant) {
    sleep_run_from_xosc();
  } else {
    sleep_run_from_lposc();
  }

  // Configure aon_timer alarm
  struct timespec ts;
  aon_timer_get_time(&ts);
  ts.tv_sec += max_period_ms / 1000;

  // Enter sleep/dormant mode until the AON timer alarm fires
  if (!is_dormant) {
    sleep_goto_sleep_until(&ts, _cb_wake);
  } else {
    sleep_goto_dormant_until(&ts, _cb_wake);
  }
}

/**************************************************************************/
/*!
    @brief  Puts the RP2350 into Dormant State (6.5.3 in RP2350 Datasheet) until
   the specified GPIO pin changes state.
    @param  gpio_pin
            GPIO pin to monitor for wakeup.
    @param  edge
            True for rising edge, False for falling edge
    @param  high
            True for active high, False for active low.
*/
/**************************************************************************/
void WatchdogRP2350::goToSleepUntilPin(uint gpio_pin, bool edge, bool high) {
  // Set the crystal oscillator as the dormant clock source, UART will be
  // reconfigured from here This is necessary before sending the pico into
  // dormancy
  // NOTE: Because we are using the crystal oscillator as the clock source, the
  // AON timer will not run, so we can not use the AON timer to measure sleep
  // duration like we do in goToSleepUntil().
  sleep_run_from_xosc();

  // Enter dormant state until the specified GPIO pin changes state
  sleep_goto_dormant_until_pin(gpio_pin, edge, high);
}

/**************************************************************************/
/*!
    @brief  Helper function to initialize and check AON timer.
    @return True if the AON timer has been started, False otherwise.
*/
bool WatchdogRP2350::startAonTimer() {
  if (!_aon_timer_started) {
    struct timespec ts_init = {.tv_sec = 1723124088, .tv_nsec = 0};
    aon_timer_start(&ts_init);
    _aon_timer_started = true;
  }
  return _aon_timer_started;
}

#endif // ARDUINO_ARCH_RP2350 || PICO_RP2350 || TARGET_RP2350
