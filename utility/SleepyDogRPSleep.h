/*!
 * @file SleepyDogRPSleep.h
 *
 * Public RP2040 / RP2350 sleep API of Adafruit SleepyDog, "config struct"
 * shape: fill a SleepyDogConfig, call Watchdog.sleep(cfg). Mixed into the
 * WatchdogRP2040 and WatchdogRP2350 classes.
 *
 * Needs arduino-pico >= 6.4.0 (LowPower library). On older cores, and in
 * Adafruit TinyUSB host-mode builds, every sleep returns
 * SLEEPYDOG_ERR_UNSUPPORTED and sleepCapabilities() is 0.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * MIT License, all text here must be included in any redistribution.
 */
#ifndef SLEEPYDOG_RP_SLEEP_H_
#define SLEEPYDOG_RP_SLEEP_H_

#include "SleepyDogRP.h"

/*! Sleep mode for SleepyDogConfig::mode. */
enum SleepyDogMode {
  /*! Light sleep: clocks gated, the sketch resumes where it stopped, RAM,
   * pins, USB (if in use) and a connected WiFi station are kept. */
  SLEEPYDOG_LIGHT = 0,
  /*! Deep sleep: lowest power, the chip reboots on wake (setup() runs
   * again); only wakeReason() and persist() words survive. RP2350: timer
   * and/or pin. RP2040: see sleepCapabilities(). */
  SLEEPYDOG_DEEP
};

/*!
 * Everything one sleep needs. Start from the defaults and set only what you
 * need, e.g. `SleepyDogConfig cfg; cfg.durationMs = 5000;`
 * At least one wake source (durationMs and/or wakePin) is required; with both,
 * whichever comes first wakes the chip.
 */
struct SleepyDogConfig {
  SleepyDogMode mode = SLEEPYDOG_LIGHT; ///< SLEEPYDOG_LIGHT or SLEEPYDOG_DEEP
  uint32_t durationMs = 0; ///< wake after this many ms, 0 = no timer wake
  int wakePin = -1;        ///< GPIO number to wake on, -1 = no pin wake
  /*! LOW / HIGH (level: wakes at once if already at that level) or
   * RISING / FALLING (edge). */
  PinStatus wakeOn = LOW;
  /*! Pull applied to wakePin before sleeping. With PULL_KEEP the pin is
   * left as configured. RP2350: no internal pull-down with HIGH/RISING
   * (erratum RP2350-E9), that returns SLEEPYDOG_ERR_PULLDOWN_ERRATA. */
  SleepyDogPull wakePull = SLEEPYDOG_PULL_KEEP;
  /*! Radio policy (on-board CYW43 only). IGNORE (default) never refuses;
   * SLEEPYDOG_RADIO_MUST_BE_OFF refuses with SLEEPYDOG_ERR_RADIO_ON while the
   * chip is powered. Call WiFi.powerOff() first for the lowest current. */
  SleepyDogRadio radio = SLEEPYDOG_RADIO_IGNORE;
  /*! RP2040 deep sleep only: bit n set = leave GPIO n as it is (other
   * unused pins are parked in a low-leakage state while dormant). */
  uint64_t keepPins = 0;
  /*! Light sleep only: SLEEPYDOG_KEEP_* peripherals kept running. */
  uint32_t keepRunning = 0;
};

/**************************************************************************/
/*!
    @brief  RP2040 / RP2350 sleep API (config struct shape), inherited by the
            RP watchdog classes and used as Watchdog.sleep(cfg) etc.
*/
/**************************************************************************/
class SleepyDogRPSleep {
public:
  int sleep(const SleepyDogConfig &cfg);
  uint32_t sleepCapabilities();
  int wakeReason(uint32_t *sleptMs = nullptr);
  bool persist(uint32_t *words, uint8_t count, bool write);
  const char *sleepErrorString(int rc);
  bool radioIsOn();
};

#endif // SLEEPYDOG_RP_SLEEP_H_
