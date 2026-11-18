/*!
 * @file SleepyDogRPSleep.cpp
 *
 * Public RP2040 / RP2350 sleep API of Adafruit SleepyDog (one method per
 * mode). See SleepyDogRPSleep.h.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * MIT License, all text here must be included in any redistribution.
 */
#if defined(ARDUINO_ARCH_RP2040)

#include "SleepyDogRPSleep.h"

// Every sleep method ends up here, with the options set by the setters
int SleepyDogRPSleep::sleepRequest(bool deep, uint32_t ms, int pin,
                                   PinStatus mode, SleepyDogPull pull) {
  const SleepyDogRPRequest req = {deep, ms,           pin,       mode,
                                  pull, _radioPolicy, _keepPins, _keepRunning};
  return sleepydog_rp_sleep(req);
}

/**************************************************************************/
/*!
    @brief  Light sleep for a time: clocks gated, the sketch resumes right
            after the call, RAM, pins, USB (while a host uses it) and a
            connected WiFi station are kept. A watchdog enabled with
            Watchdog.enable() is paused and re-armed with the same timeout.
    @param  ms
            Time to sleep in ms (must not be 0).
    @return 0 on success, otherwise a SLEEPYDOG_ERR_* or PICO_ERROR_* code
            (see sleepErrorString()).
*/
/**************************************************************************/
int SleepyDogRPSleep::lightSleep(uint32_t ms) {
  return sleepRequest(false, ms, -1, LOW, SLEEPYDOG_PULL_KEEP);
}

/**************************************************************************/
/*!
    @brief  Light sleep until a pin condition, or a timeout, whichever comes
            first (wakeReason() says which).
    @param  pin
            GPIO number to wake on.
    @param  mode
            LOW / HIGH (level: returns at once if the pin is already there)
            or RISING / FALLING (edge).
    @param  pull
            Pull applied to the pin first (SLEEPYDOG_PULL_KEEP: leave it as
            configured). The pin's function is not changed, so a UART RX pin
            can wake the board. RP2350: no internal pull-down with
            HIGH/RISING (erratum RP2350-E9).
    @param  timeoutMs
            Longest time to sleep in ms, 0 = wait for the pin only.
    @return 0 on success, otherwise an error code (see sleepErrorString()).
*/
/**************************************************************************/
int SleepyDogRPSleep::lightSleepUntilPin(int pin, PinStatus mode,
                                         SleepyDogPull pull,
                                         uint32_t timeoutMs) {
  return sleepRequest(false, timeoutMs, pin < 0 ? -2 : pin, mode, pull);
}

/**************************************************************************/
/*!
    @brief  Deep sleep for a time (RP2350: chip powered down). Does not
            return on success: the chip reboots on wake and setup() runs
            again; call wakeReason() there. Power the WiFi chip off first
            (WiFi.disconnect(), WiFi.powerOff()) or it keeps drawing current;
            it only blocks the sleep with SLEEPYDOG_RADIO_MUST_BE_OFF. Flush
            Serial (and wait ~5 ms) first: USB is dropped. RP2040: not
            supported.
    @param  ms
            Time to sleep in ms, at least 10.
    @return Only on error: a SLEEPYDOG_ERR_* or PICO_ERROR_* code.
*/
/**************************************************************************/
int SleepyDogRPSleep::deepSleep(uint32_t ms) {
  return sleepRequest(true, ms, -1, LOW, SLEEPYDOG_PULL_KEEP);
}

/**************************************************************************/
/*!
    @brief  Deep sleep until a pin condition, or optionally a timeout
            (RP2350). Does not return on success: the chip reboots on wake.
            Same rules as deepSleep().
    @param  pin
            GPIO number to wake on.
    @param  mode
            LOW, HIGH, RISING or FALLING.
    @param  pull
            Pull applied to the pin first, see lightSleepUntilPin().
    @param  timeoutMs
            Also wake after this many ms (RP2350 only), 0 = pin only.
    @return Only on error: a SLEEPYDOG_ERR_* or PICO_ERROR_* code.
*/
/**************************************************************************/
int SleepyDogRPSleep::deepSleepUntilPin(int pin, PinStatus mode,
                                        SleepyDogPull pull,
                                        uint32_t timeoutMs) {
  return sleepRequest(true, timeoutMs, pin < 0 ? -2 : pin, mode, pull);
}

/**************************************************************************/
/*!
    @brief  What the following sleeps require of the WiFi chip (Pico W,
            Pico 2 W). SleepyDog never powers it on or off itself.
    @param  policy
            SLEEPYDOG_RADIO_IGNORE (default): never refuse;
            SLEEPYDOG_RADIO_MUST_BE_OFF: every sleep returns
            SLEEPYDOG_ERR_RADIO_ON while the on-board CYW43 is powered.
*/
/**************************************************************************/
void SleepyDogRPSleep::setRadioPolicy(SleepyDogRadio policy) {
  _radioPolicy = policy;
}

/**************************************************************************/
/*!
    @brief  RP2040 deep sleep only: GPIOs to leave untouched while dormant
            (other unused pins are parked in a low-leakage state). Ignored
            by light sleep, which never parks pins.
    @param  mask
            Bit n set = leave GPIO n alone.
*/
/**************************************************************************/
void SleepyDogRPSleep::keepPins(uint64_t mask) { _keepPins = mask; }

/**************************************************************************/
/*!
    @brief  Peripherals that keep running during the following light sleeps
            (keeps PLL_SYS on, about 4 mA more).
    @param  mask
            SLEEPYDOG_KEEP_PWM, _PIO, _UART0, _UART1, _ADC, _DMA OR-ed
            together; 0 (default) = none.
*/
/**************************************************************************/
void SleepyDogRPSleep::keepRunning(uint32_t mask) { _keepRunning = mask; }

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
    @brief  Human readable text for a sleep method's return code.
    @param  rc
            Value returned by a sleep method.
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
