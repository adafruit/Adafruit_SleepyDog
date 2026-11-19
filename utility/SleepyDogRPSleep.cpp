/*!
 * @file SleepyDogRPSleep.cpp
 *
 * Public RP2040 / RP2350 sleep API of Adafruit SleepyDog (config struct
 * shape). See SleepyDogRPSleep.h.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * MIT License, all text here must be included in any redistribution.
 */
#if defined(ARDUINO_ARCH_RP2040)

#include "SleepyDogRPSleep.h"

/**************************************************************************/
/*!
    @brief  Sleep as described by cfg.

            SLEEPYDOG_LIGHT returns after the wake. SLEEPYDOG_DEEP does not
            return on success: the chip reboots on wake and setup() runs
            again; call wakeReason() there. A hardware watchdog enabled with
            Watchdog.enable() is paused for the sleep (re-armed after a light
            sleep, off after a deep sleep reboot). SleepyDog never powers the
            WiFi chip: call WiFi.powerOff() first and WiFi.powerOn() after.
            Flush Serial (and wait ~5 ms) before a deep sleep: USB is dropped.
    @param  cfg
            What to sleep for, see SleepyDogConfig.
    @return 0 after a light sleep, otherwise a SLEEPYDOG_ERR_* code or a Pico
            SDK PICO_ERROR_* code (see sleepErrorString()); nothing was slept.
*/
/**************************************************************************/
int SleepyDogRPSleep::sleep(const SleepyDogConfig &cfg) {
  const SleepyDogRPRequest req = {cfg.mode == SLEEPYDOG_DEEP,
                                  cfg.durationMs,
                                  cfg.wakePin,
                                  cfg.wakeOn,
                                  cfg.wakePull,
                                  cfg.radio,
                                  cfg.keepPins,
                                  cfg.keepRunning};
  return sleepydog_rp_sleep(req);
}

/**************************************************************************/
/*!
    @brief  What this chip and core can do.
    @return SLEEPYDOG_CAP_* bits OR-ed together, 0 if sleep is unavailable
            (core < 6.4.0, TinyUSB host mode, FreeRTOS).
*/
/**************************************************************************/
uint32_t SleepyDogRPSleep::sleepCapabilities() {
  return sleepydog_rp_capabilities();
}

/**************************************************************************/
/*!
    @brief  What ended the last sleep. After a deep sleep this is valid from
            the start of setup() (SLEEPYDOG_WAKE_DEEP_TIMER / _DEEP_PIN)
            until the next sleep.
    @param  sleptMs
            If not NULL, receives how long it slept in ms (RP2350 deep: AON
            timer, includes the boot up to static init),
            SLEEPYDOG_SLEPT_UNKNOWN when no clock ran (RP2040 deep), 0 if
            nothing was slept.
    @return A SleepyDogWake value; SLEEPYDOG_WAKE_NONE after a normal reset
            or a failed sleep.
*/
/**************************************************************************/
int SleepyDogRPSleep::wakeReason(uint32_t *sleptMs) {
  return sleepydog_rp_wake_reason(sleptMs);
}

/**************************************************************************/
/*!
    @brief  Store or read up to SLEEPYDOG_PERSIST_WORDS (2) 32-bit words that
            survive deep sleep and soft resets (watchdog, reboot), but not a
            power cycle or the RUN pin. Anything bigger belongs in flash
            (EEPROM / LittleFS).
    @param  words
            Words to store, or buffer that receives the stored words.
    @param  count
            Number of words, 1 or 2.
    @param  write
            true to store, false to read.
    @return true on success; on read, false if nothing was stored since
            power-on (buffer zeroed). Always false on cores < 6.4.0.
*/
/**************************************************************************/
bool SleepyDogRPSleep::persist(uint32_t *words, uint8_t count, bool write) {
  return sleepydog_rp_persist(words, count, write);
}

/**************************************************************************/
/*!
    @brief  Human readable text for a sleep() return code.
    @param  rc
            Value returned by sleep().
    @return Constant string, never NULL.
*/
/**************************************************************************/
const char *SleepyDogRPSleep::sleepErrorString(int rc) {
  return sleepydog_rp_error_string(rc);
}

/**************************************************************************/
/*!
    @brief  Is the on-board CYW43 WiFi/Bluetooth chip (Pico W, Pico 2 W,
            other boards whose core definition has one) powered? The same
            test the radio policy uses. AirLift / ESP32 coprocessors are not
            seen.
    @return true if powered; false after WiFi.powerOff(), on boards without
            the chip, and on cores < 6.4.0.
*/
/**************************************************************************/
bool SleepyDogRPSleep::radioIsOn() { return sleepydog_rp_radio_on(); }

#endif // ARDUINO_ARCH_RP2040
