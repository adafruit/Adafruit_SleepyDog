#if defined(ARDUINO_ARCH_RP2350) || defined(PICO_RP2350) ||                    \
    defined(TARGET_RP2350)

#include "WatchdogRP2350/WatchdogRP2350.h"
#include "SleepyDogRP.h"
#include <hardware/gpio.h>

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
void WatchdogRP2350::reset() { watchdog_update(); }

/**************************************************************************/
/*!
    @brief  Disables the RP2350's hardware watchdog timer (clears
            WATCHDOG_CTRL.ENABLE, the same as the SDK's watchdog_disable()).
*/
/**************************************************************************/
void WatchdogRP2350::disable() {
  hw_clear_bits(&watchdog_hw->ctrl, WATCHDOG_CTRL_ENABLE_BITS);
  _wdto = -1;
  sleepydog_rp_wdt_note(0);
}

/**************************************************************************/
/*!
    @brief  Puts the RP2350 into light sleep for a period of time.

            With arduino-pico >= 6.4.0 this is a real low-power sleep
            (LowPower.sleepFor(): clocks gated, PLLs stopped, the sketch
            resumes where it stopped, RAM and pins kept). A watchdog
            enabled with enable() is paused for the sleep and re-armed with
            the same timeout afterwards. On older cores, with FreeRTOS or in
            TinyUSB host-mode builds it falls back to sleep_ms().
    @param    maxPeriodMS
              Time to sleep the RP2350, in millis. 0 returns at once.
    @return The period (in milliseconds) that the hardware was asleep, 0 if
            it did not sleep.
*/
/**************************************************************************/
int WatchdogRP2350::sleep(int maxPeriodMS) {
  return sleepydog_rp_watchdog_sleep(maxPeriodMS);
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

#if SLEEPYDOG_RP_LOWPOWER
// ---------------------------------------------------------------------------
// Legacy RP2350 sleep API on arduino-pico >= 6.4.0, built on LowPower. The
// LowPower calls hold the CYW43 driver lock while the clocks are lowered, which
// should let a Pico 2 W with WiFi connected wake up again (the vendored
// pico_sleep code did not, see adafruit/Adafruit_SleepyDog#72; not yet
// confirmed on hardware), and they restore the clocks themselves. The hardware
// watchdog is paused for the sleep and re-armed.
// ---------------------------------------------------------------------------

/**************************************************************************/
/*!
    @brief  Kept for compatibility. With arduino-pico >= 6.4.0 the clocks are
            restored by the sleep call itself, so this only re-attaches USB
            on boards where that applies.
*/
/**************************************************************************/
void WatchdogRP2350::resumeFromSleep() {
#if defined(USBCON) && !defined(USE_TINYUSB)
  USBDevice.attach();
#endif
}

/**************************************************************************/
/*!
    @brief  Gets the duration of the previous sleep cycle, in milliseconds.
    @return The sleep duration in milliseconds, -1 if it is not known (pin
            wake from dormant: no clock runs to measure it).
*/
/**************************************************************************/
long WatchdogRP2350::getSleepDuration() {
  uint32_t ms = 0;
  sleepydog_rp_wake_reason(&ms);
  return ms == SLEEPYDOG_SLEPT_UNKNOWN ? -1L : (long)ms;
}

/**************************************************************************/
/*!
    @brief  Sleeps for a specified period of time, then calls the wake
            callback (setWakeCb()) and returns.
    @param  max_period_ms
            Desired sleep period, in milliseconds.
    @param  is_dormant
            false (default): light sleep, LowPower.sleepFor(); USB and a
            connected WiFi station survive it. true: dormant,
            LowPower.dormantFor() (oscillators stopped, USB drops and
            re-enumerates; at least 10 ms). Repeated dormant cycles in an
            Adafruit TinyUSB build need Adafruit TinyUSB >= 3.8.0 (3.7.7, as
            bundled with core 6.4.0, panics on the 3rd wake).
*/
/**************************************************************************/
void WatchdogRP2350::goToSleepUntil(int max_period_ms, bool is_dormant) {
  if (max_period_ms < 0)
    return;
  if (is_dormant) {
    sleepydog_rp_legacy_dormant(-1, LOW, (uint32_t)max_period_ms);
  } else {
    SleepyDogRPRequest req = {false,
                              (uint32_t)max_period_ms,
                              -1,
                              LOW,
                              SLEEPYDOG_PULL_KEEP,
                              SLEEPYDOG_RADIO_IGNORE,
                              0,
                              0};
    sleepydog_rp_sleep(req);
  }
  if (_cb_wake)
    _cb_wake();
}

/**************************************************************************/
/*!
    @brief  Puts the RP2350 into dormant (LowPower.dormantUntilPin()) until
            the GPIO pin condition, then calls the wake callback and returns.
            The pin's pull is left as configured (set it with pinMode()
            first; RP2350-E9: use an external pull-down for a HIGH/RISING
            wake). USB drops and re-enumerates after the wake.
    @param  gpio_pin
            GPIO pin to monitor for wakeup.
    @param  edge
            true: wake on an edge, false: wake on a level.
    @param  high
            true: rising edge / high level, false: falling edge / low level.
*/
/**************************************************************************/
void WatchdogRP2350::goToSleepUntilPin(uint gpio_pin, bool edge, bool high) {
  const PinStatus mode = edge ? (high ? RISING : FALLING) : (high ? HIGH : LOW);
  // an unconfigured RP2350 pad is isolated and could never wake the chip
  if (gpio_pin < NUM_BANK0_GPIOS &&
      gpio_get_function(gpio_pin) == GPIO_FUNC_NULL) {
    gpio_init(gpio_pin);
  }
  sleepydog_rp_legacy_dormant((int)gpio_pin, mode, 0);
  if (_cb_wake)
    _cb_wake();
}

#else // !SLEEPYDOG_RP_LOWPOWER: vendored pico_sleep (core < 6.4.0, host mode)

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
            true: wake on an edge, false: wake on a level.
    @param  high
            true: rising edge / high level, false: falling edge / low level.
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

#endif // SLEEPYDOG_RP_LOWPOWER

#endif // ARDUINO_ARCH_RP2350 || PICO_RP2350 || TARGET_RP2350
