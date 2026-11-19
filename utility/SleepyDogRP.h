/*!
 * @file SleepyDogRP.h
 *
 * RP2040 / RP2350 sleep engine shared by the Adafruit SleepyDog RP watchdog
 * classes, plus the public SLEEPYDOG_* enums used by the RP sleep API.
 *
 * The engine is built on the arduino-pico LowPower library (core >= 6.4.0):
 *  - light sleep (both chips) = LowPower.sleepFor() / sleepUntilPin(): the
 *    sketch resumes where it stopped, RAM and pins are kept;
 *  - deep sleep on RP2350 = SDK power-down (Pstate) woken by the AON timer
 *    and/or a GPIO; the chip reboots on wake, only a small wake record and
 *    two user words survive (powman scratch registers).
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * MIT License, all text here must be included in any redistribution.
 */
#ifndef SLEEPYDOG_RP_H_
#define SLEEPYDOG_RP_H_

#include "SleepyDogRPGate.h"
#include <Arduino.h>

/*! Pull applied to the wake pin before sleeping. */
enum SleepyDogPull {
  SLEEPYDOG_PULL_KEEP = 0, ///< leave the pin's pull-up/down as configured
  SLEEPYDOG_PULL_NONE,     ///< disable both pulls (external pull fitted)
  SLEEPYDOG_PULL_UP,       ///< internal pull-up (use with LOW / FALLING)
  SLEEPYDOG_PULL_DOWN ///< internal pull-down (not with HIGH/RISING on RP2350)
};

/*! What a sleep requires of the on-board CYW43 WiFi/Bluetooth chip (Pico W,
 * Pico 2 W and other boards whose core definition has one). SleepyDog never
 * switches the radio itself and by default never refuses because of it. A
 * powered chip keeps drawing current through the sleep: power it off with
 * WiFi.powerOff() before sleeping and back on with WiFi.powerOn() after.
 * AirLift / ESP32 coprocessors (e.g. Fruit Jam) are not seen. */
enum SleepyDogRadio {
  SLEEPYDOG_RADIO_IGNORE = 0, ///< default: never refuse because of the radio
  SLEEPYDOG_RADIO_MUST_BE_OFF ///< refuse to sleep while the radio is powered
};

/*! What ended the last sleep. */
enum SleepyDogWake {
  SLEEPYDOG_WAKE_NONE = 0,   ///< no sleep yet, or the last sleep failed
  SLEEPYDOG_WAKE_TIMER,      ///< light sleep ended by its timer
  SLEEPYDOG_WAKE_PIN,        ///< light sleep ended by the wake pin
  SLEEPYDOG_WAKE_DEEP_TIMER, ///< this boot is a deep sleep wake by the timer
  SLEEPYDOG_WAKE_DEEP_PIN    ///< this boot is a deep sleep wake by the pin
};

/*! Capability bits, see sleepCapabilities(). */
enum SleepyDogCapability {
  SLEEPYDOG_CAP_LIGHT_TIMER = 1u << 0,     ///< light sleep, timer wake
  SLEEPYDOG_CAP_LIGHT_PIN = 1u << 1,       ///< light sleep, pin wake
  SLEEPYDOG_CAP_LIGHT_PIN_TIMER = 1u << 2, ///< light sleep, pin or timer
  SLEEPYDOG_CAP_DEEP_TIMER = 1u << 3,      ///< deep sleep, timer wake
  SLEEPYDOG_CAP_DEEP_PIN = 1u << 4,        ///< deep sleep, pin wake
  SLEEPYDOG_CAP_DEEP_PIN_TIMER = 1u << 5,  ///< deep sleep, pin or timer
  /*! deep sleep draws much less than light sleep (worth the reboot) */
  SLEEPYDOG_CAP_DEEP_LOW_POWER = 1u << 6
};

/*! Peripherals kept running during light sleep (OR them together). Same
 * values as LowPowerClass::KR_*. Any of them keeps PLL_SYS on (~4 mA). */
enum SleepyDogKeepRunning {
  SLEEPYDOG_KEEP_PWM = 1,   ///< PWM slices
  SLEEPYDOG_KEEP_PIO = 2,   ///< PIO blocks
  SLEEPYDOG_KEEP_UART0 = 4, ///< UART0 (Serial1)
  SLEEPYDOG_KEEP_UART1 = 8, ///< UART1 (Serial2)
  SLEEPYDOG_KEEP_ADC = 16,  ///< ADC (and DMA, PLL_USB: ~5 mA more)
  SLEEPYDOG_KEEP_DMA = 32   ///< DMA
};

/*! SleepyDog's own error codes. Every sleep call returns 0 on success, one
 * of these, or a Pico SDK PICO_ERROR_* code (all > -100) passed through from
 * LowPower / the SDK, e.g. PICO_ERROR_NOT_PERMITTED with FreeRTOS. */
enum SleepyDogError {
  SLEEPYDOG_OK = 0, ///< success
  /*! this mode / wake combination is not available on this chip or core */
  SLEEPYDOG_ERR_UNSUPPORTED = -100,
  SLEEPYDOG_ERR_NO_WAKE_SOURCE = -101, ///< neither a duration nor a wake pin
  SLEEPYDOG_ERR_BAD_PIN = -102,        ///< wake pin number out of range
  SLEEPYDOG_ERR_BAD_WAKE_MODE = -103,  ///< not LOW, HIGH, RISING or FALLING
  SLEEPYDOG_ERR_RADIO_ON = -104, ///< radio powered and policy is MUST_BE_OFF:
                                 ///< call WiFi.powerOff() first
  /*! RP2350-E9: internal pull-down with a HIGH/RISING wake is unreliable;
   * use an external pull-down, or FALLING/LOW with a pull-up */
  SLEEPYDOG_ERR_PULLDOWN_ERRATA = -105,
  SLEEPYDOG_ERR_TOO_SHORT = -106 ///< deep sleep shorter than 10 ms
};

/*! Slept time when it cannot be measured (RP2040 dormant pin wake). */
#define SLEEPYDOG_SLEPT_UNKNOWN 0xFFFFFFFFu

/*! Number of 32-bit words persist() keeps across deep sleep and resets. */
#define SLEEPYDOG_PERSIST_WORDS 2

/*! Internal: one sleep request handed from the public API to the engine. */
struct SleepyDogRPRequest {
  bool deep;            ///< true = deep sleep (reboots on wake)
  uint32_t durationMs;  ///< timer wake after this many ms, 0 = no timer
  int wakePin;          ///< GPIO to wake on, -1 = none
  PinStatus wakeOn;     ///< LOW, HIGH, RISING or FALLING
  SleepyDogPull pull;   ///< pull to apply to the wake pin
  SleepyDogRadio radio; ///< radio policy
  uint64_t keepPins;    ///< RP2040 deep sleep: GPIOs not parked
  uint32_t keepRunning; ///< light sleep: SLEEPYDOG_KEEP_* mask
};

int sleepydog_rp_sleep(const SleepyDogRPRequest &req);
uint32_t sleepydog_rp_capabilities(void);
int sleepydog_rp_wake_reason(uint32_t *sleptMs);
bool sleepydog_rp_persist(uint32_t *words, uint8_t count, bool write);
const char *sleepydog_rp_error_string(int rc);
bool sleepydog_rp_radio_on(void);
void sleepydog_rp_wdt_note(int timeoutMs);
int sleepydog_rp_watchdog_sleep(int ms);
int sleepydog_rp_legacy_dormant(int pin, PinStatus mode, uint32_t ms);

#endif // SLEEPYDOG_RP_H_
