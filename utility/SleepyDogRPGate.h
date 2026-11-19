/*!
 * @file SleepyDogRPGate.h
 *
 * Compile-time gate for the RP2040/RP2350 sleep engine of Adafruit SleepyDog.
 * Plain C header (no Arduino.h), so the vendored pico_sleep C files can use it.
 *
 * SLEEPYDOG_RP_LOWPOWER is 1 when the arduino-pico core is 6.4.0 or newer
 * (the core ships the LowPower library and the SDK pico_low_power API) and the
 * build is not an Adafruit TinyUSB host-mode build. The core version is used,
 * not __has_include(<LowPower.h>): under arduino-cli that test is false until
 * the LowPower library has been discovered.
 *
 * Host mode is excluded because LowPower pulls in libpico's TinyUSB device
 * objects, which collide with Adafruit TinyUSB's host build at link time (28
 * duplicate symbols on core 6.4.0) until the core gains weak tud_* stubs.
 * Without the exclusion a host-mode sketch that only uses the watchdog would
 * stop linking.
 *
 * MIT License, all text here must be included in any redistribution.
 */
#ifndef SLEEPYDOG_RP_GATE_H_
#define SLEEPYDOG_RP_GATE_H_

#if defined(__has_include)
#if __has_include(<RP2040Version.h>)
#include <RP2040Version.h>
#endif
#endif

#if defined(ARDUINO_PICO_MAJOR) &&                                             \
    ((ARDUINO_PICO_MAJOR > 6) ||                                               \
     (ARDUINO_PICO_MAJOR == 6 && ARDUINO_PICO_MINOR >= 4))
/*! The arduino-pico core is 6.4.0 or newer (has LowPower). */
#define SLEEPYDOG_RP_CORE_HAS_LOWPOWER 1
#else
/*! The arduino-pico core is older than 6.4.0 (no LowPower). */
#define SLEEPYDOG_RP_CORE_HAS_LOWPOWER 0
#endif

#if SLEEPYDOG_RP_CORE_HAS_LOWPOWER &&                                          \
    !(defined(USE_TINYUSB) && defined(USE_TINYUSB_HOST))
/*! The LowPower based sleep engine is built. */
#define SLEEPYDOG_RP_LOWPOWER 1
#else
/*! The LowPower based sleep engine is not built (old core or host mode). */
#define SLEEPYDOG_RP_LOWPOWER 0
#endif

#endif // SLEEPYDOG_RP_GATE_H_
