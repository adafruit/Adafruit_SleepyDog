/*!
 * @file WatchdogRP2350.h
 *
 * Support for RP2350 Hardware Watchdog Timer API
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * Written by Brent Rubell for Adafruit Industries.
 *
 * MIT License, all text here must be included in any redistribution.
 *
 */
#ifndef WATCHDOGRP2350_H_
#define WATCHDOGRP2350_H_

#include "../SleepyDogRPGate.h"
#include <hardware/watchdog.h>
#include <pico/time.h>
#if !SLEEPYDOG_RP_LOWPOWER
// vendored pico_sleep, only for cores without LowPower (or host mode)
#include "pico/aon_timer.h"
#include "pico_sleep/include/pico/sleep.h"
#endif

// Callback type for wake-from-sleep functions
typedef void (*WakeCb)(void);

/**************************************************************************/
/*!
    @brief  Class that contains functions for interacting with the
            RP2350's hardware watchdog timer and low-power sleep functionality.
*/
/**************************************************************************/
class WatchdogRP2350 {
public:
  WatchdogRP2350() : _wdto(-1) {}
  // Watchdog API
  int enable(int maxPeriodMS = 0);
  void disable();
  void reset();
  // Low-power ("sleep") mode API
  int sleep(
      int maxPeriodMS = 0); // Basic sleep function, handled by software timer
  // Advanced "Sleep" State API
  // Sleep State (6.5.2, 6.5.3 in RP2350 Datasheet)
  void goToSleepUntil(int max_period_ms = 0, bool is_dormant = false);
  void goToSleepUntilPin(uint gpio_pin, bool edge = false, bool high = false);
  // Advanced sleep state helper functions
  void setWakeCb(WakeCb cb);
  void resumeFromSleep();
  long getSleepDuration();

private:
  int _wdto;
  WakeCb _cb_wake = nullptr;
#if !SLEEPYDOG_RP_LOWPOWER
  struct timespec _ts_sleep_start;
  bool _aon_timer_started = false;
  bool startAonTimer();
#endif
};

#endif // WATCHDOGRP2350_H_