/*!
 * @file SleepyDogRPSleep.h
 *
 * Public RP2040 / RP2350 sleep API of Adafruit SleepyDog, "one method per
 * mode" shape: Watchdog.lightSleep(ms), Watchdog.deepSleepUntilPin(...) etc.,
 * with setters for the options that rarely change. Mixed into the
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

/**************************************************************************/
/*!
    @brief  RP2040 / RP2350 sleep API (one method per mode), inherited by the
            RP watchdog classes and used as Watchdog.lightSleep(ms) etc.
*/
/**************************************************************************/
class SleepyDogRPSleep {
public:
  // Light sleep: resumes after the wake
  int lightSleep(uint32_t ms);
  int lightSleepUntilPin(int pin, PinStatus mode,
                         SleepyDogPull pull = SLEEPYDOG_PULL_KEEP,
                         uint32_t timeoutMs = 0);
  // Deep sleep: reboots on wake, does not return on success
  int deepSleep(uint32_t ms);
  int deepSleepUntilPin(int pin, PinStatus mode,
                        SleepyDogPull pull = SLEEPYDOG_PULL_KEEP,
                        uint32_t timeoutMs = 0);
  // Options kept for every following sleep
  void setRadioPolicy(SleepyDogRadio policy);
  void keepPins(uint64_t mask);
  void keepRunning(uint32_t mask);
  // Queries
  uint32_t sleepCapabilities();
  int wakeReason(uint32_t *sleptMs = nullptr);
  bool persist(uint32_t *words, uint8_t count, bool write);
  const char *sleepErrorString(int rc);
  bool radioIsOn();

private:
  int sleepRequest(bool deep, uint32_t ms, int pin, PinStatus mode,
                   SleepyDogPull pull);
  SleepyDogRadio _radioPolicy = SLEEPYDOG_RADIO_IGNORE;
  uint64_t _keepPins = 0;
  uint32_t _keepRunning = 0;
};

#endif // SLEEPYDOG_RP_SLEEP_H_
