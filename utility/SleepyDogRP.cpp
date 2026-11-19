/*!
 * @file SleepyDogRP.cpp
 *
 * RP2040 / RP2350 sleep engine for Adafruit SleepyDog, built on the
 * arduino-pico LowPower library (core >= 6.4.0). See SleepyDogRP.h.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * MIT License, all text here must be included in any redistribution.
 */
#if defined(ARDUINO_ARCH_RP2040)

#include "SleepyDogRP.h"
#include <hardware/gpio.h>
#include <hardware/structs/watchdog.h>
#include <hardware/watchdog.h>
#include <pico/error.h>
#include <pico/time.h>

#if SLEEPYDOG_RP_LOWPOWER
#include <LowPower.h>
#if PICO_RP2350
#include <hardware/powman.h>
#include <pico/aon_timer.h>
#include <pico/low_power.h>
#endif
#if defined(PICO_CYW43_SUPPORTED)
#include <pico/cyw43_arch.h>
#endif

// Present only if the sketch defines them (same test as the core and
// LowPower use to know whether core 1 is running)
extern void setup1() __attribute__((weak));
extern void loop1() __attribute__((weak));

static_assert((int)SLEEPYDOG_KEEP_PWM == (int)LowPowerClass::KR_PWM &&
                  (int)SLEEPYDOG_KEEP_PIO == (int)LowPowerClass::KR_PIO &&
                  (int)SLEEPYDOG_KEEP_UART0 == (int)LowPowerClass::KR_UART0 &&
                  (int)SLEEPYDOG_KEEP_UART1 == (int)LowPowerClass::KR_UART1 &&
                  (int)SLEEPYDOG_KEEP_ADC == (int)LowPowerClass::KR_ADC &&
                  (int)SLEEPYDOG_KEEP_DMA == (int)LowPowerClass::KR_DMA,
              "SLEEPYDOG_KEEP_* must match LowPowerClass::KR_*");
#endif // SLEEPYDOG_RP_LOWPOWER

// Timeout given to Watchdog.enable() (0 = not enabled through SleepyDog)
static int s_wdtTimeoutMs = 0;
// What ended the last sleep and how long it was (see wake_reason)
static SleepyDogWake s_reason = SLEEPYDOG_WAKE_NONE;
static uint32_t s_sleptMs = 0;

// persist(): marker word saying the user words are valid
#define SD_PERSIST_MAGIC 0x5D0DDA7Au

/**************************************************************************/
/*!
    @brief  Remember the timeout the watchdog was enabled with, so it can be
            re-armed with the same timeout after a light sleep.
    @param  timeoutMs
            Timeout passed to Watchdog.enable(), 0 when disabled.
*/
/**************************************************************************/
void sleepydog_rp_wdt_note(int timeoutMs) { s_wdtTimeoutMs = timeoutMs; }

/**************************************************************************/
/*!
    @brief  Human readable text for a sleep call's return code.
    @param  rc
            Return code of a SleepyDog sleep call.
    @return Constant string, never NULL.
*/
/**************************************************************************/
const char *sleepydog_rp_error_string(int rc) {
  switch (rc) {
  case SLEEPYDOG_OK:
    return "OK";
  case SLEEPYDOG_ERR_UNSUPPORTED:
    return "not supported on this chip/core (see sleepCapabilities())";
  case SLEEPYDOG_ERR_NO_WAKE_SOURCE:
    return "no wake source: give a duration and/or a wake pin";
  case SLEEPYDOG_ERR_BAD_PIN:
    return "wake pin number out of range";
  case SLEEPYDOG_ERR_BAD_WAKE_MODE:
    return "wake mode must be LOW, HIGH, RISING or FALLING";
  case SLEEPYDOG_ERR_RADIO_ON:
    return "WiFi chip is powered and the radio policy is MUST_BE_OFF: call "
           "WiFi.powerOff() before sleeping";
  case SLEEPYDOG_ERR_PULLDOWN_ERRATA:
    return "RP2350-E9: no internal pull-down with a HIGH/RISING wake; use an "
           "external pull-down, or a pull-up with LOW/FALLING";
  case SLEEPYDOG_ERR_TOO_SHORT:
    return "deep sleep must be at least 10 ms";
  case PICO_ERROR_NOT_PERMITTED:
    return "not permitted (LowPower does not support FreeRTOS)";
  case PICO_ERROR_INVALID_ARG:
    return "invalid argument (Pico SDK)";
  case PICO_ERROR_INSUFFICIENT_RESOURCES:
    return "no free hardware timer alarm";
  case PICO_ERROR_PRECONDITION_NOT_MET:
    return "precondition not met (Pico SDK)";
  case PICO_ERROR_INVALID_STATE:
    return "invalid power state (Pico SDK)";
  default:
    return rc < 0 ? "Pico SDK error" : "unknown result";
  }
}

/**************************************************************************/
/*!
    @brief  What ended the last sleep (or, after a deep sleep reboot, what
            woke the chip) and how long it slept.
    @param  sleptMs
            If not NULL, receives the slept time in ms, 0 if nothing was
            slept, or SLEEPYDOG_SLEPT_UNKNOWN.
    @return A SleepyDogWake value.
*/
/**************************************************************************/
int sleepydog_rp_wake_reason(uint32_t *sleptMs) {
  if (sleptMs) {
    *sleptMs = s_sleptMs;
  }
  return s_reason;
}

#if SLEEPYDOG_RP_LOWPOWER

#if PICO_RP2350
// Deep sleep wake record in powman scratch 0..2 (the SDK owns 6 and 7):
//   [0] magic | flags | wake pin << 8   [1] requested ms   [2] AON ms at start
#define SD_REC_MAGIC 0x5D060000u
#define SD_REC_MAGIC_MASK 0xFFFF0000u
#define SD_REC_ARMED 0x1u
#define SD_REC_PIN 0x2u
#define SD_REC_TIMER 0x4u
#define SD_REC_PIN_SHIFT 8
#define SD_WDT_MAX_MS 16777
#else
#define SD_WDT_MAX_MS 8388
#endif

/**************************************************************************/
/*!
    @brief  Is the CYW43 WiFi/Bluetooth chip powered? Same test as the core's
            LowPower library: driver initialised and WL_ON driven high.
    @return true if powered, always false on boards without the chip.
*/
/**************************************************************************/
bool sleepydog_rp_radio_on(void) {
#if defined(PICO_CYW43_SUPPORTED)
  return cyw43_is_initialized(&cyw43_state) &&
         gpio_get_out_level(CYW43_PIN_WL_REG_ON);
#else
  return false;
#endif
}

// Arduino PinStatus -> the SDK's (edge, high) pair
static bool sdMapMode(PinStatus mode, bool *edge, bool *high) {
  switch (mode) {
  case LOW:
    *edge = false;
    *high = false;
    return true;
  case HIGH:
    *edge = false;
    *high = true;
    return true;
  case RISING:
    *edge = true;
    *high = true;
    return true;
  case FALLING:
    *edge = true;
    *high = false;
    return true;
  default:
    return false; // CHANGE: not supported by the hardware wake logic
  }
}

// LowPower never touches the hardware watchdog: stop it for the sleep...
static bool sdWdtPause(void) {
  const bool on = (watchdog_hw->ctrl & WATCHDOG_CTRL_ENABLE_BITS) != 0;
  if (on) {
    watchdog_disable();
  }
  return on;
}

// ...and re-arm it afterwards with the timeout it had (hardware maximum if it
// was enabled outside SleepyDog and the timeout is unknown)
static void sdWdtResume(bool wasOn) {
  if (wasOn) {
    watchdog_enable(s_wdtTimeoutMs > 0 ? s_wdtTimeoutMs : SD_WDT_MAX_MS, true);
  }
}

static void sdSetResult(int rc, SleepyDogWake why, uint32_t ms) {
  s_reason = rc ? SLEEPYDOG_WAKE_NONE : why;
  s_sleptMs = rc ? 0 : ms;
}

// Validate a request; nothing is changed yet
static int sdCheck(const SleepyDogRPRequest &r) {
#if defined(__FREERTOS)
  (void)r;
  return PICO_ERROR_NOT_PERMITTED; // as LowPower itself
#else
  const bool pin = r.wakePin >= 0;
  const bool timer = r.durationMs > 0;
  bool edge, high;
  if (r.wakePin < -1 || r.wakePin >= (int)NUM_BANK0_GPIOS) {
    return SLEEPYDOG_ERR_BAD_PIN;
  }
  if (pin && !sdMapMode(r.wakeOn, &edge, &high)) {
    return SLEEPYDOG_ERR_BAD_WAKE_MODE;
  }
  if (!pin && !timer) {
    return SLEEPYDOG_ERR_NO_WAKE_SOURCE;
  }
  if (r.deep) {
#if PICO_RP2350
    if (timer && r.durationMs < PICO_LOW_POWER_MIN_PSTATE_TIME_MS) {
      return SLEEPYDOG_ERR_TOO_SHORT;
    }
#else
    return SLEEPYDOG_ERR_UNSUPPORTED; // RP2040: no deep sleep
#endif
  }
  // Only SLEEPYDOG_RADIO_MUST_BE_OFF refuses. By default a powered WiFi chip
  // never blocks a sleep: the on-board LED of a Pico W is wired to that chip,
  // so a plain blinky has it powered too. The sketch calls WiFi.powerOff()
  // when it wants the lower current.
  if (r.radio == SLEEPYDOG_RADIO_MUST_BE_OFF && sleepydog_rp_radio_on()) {
    return SLEEPYDOG_ERR_RADIO_ON;
  }
#if PICO_RP2350
  // RP2350-E9: with the pad input enabled, a pin pulled down internally and
  // driven high latches near 2 V when released, so a HIGH level wake fires
  // again at once and a RISING edge never comes back.  Checked against the
  // pad itself, so PULL_KEEP on a pin already pulled down (the reset state of
  // an unconfigured RP2350 pin) is caught too.
  if (pin && (r.wakeOn == HIGH || r.wakeOn == RISING)) {
    if (r.pull == SLEEPYDOG_PULL_DOWN ||
        (r.pull == SLEEPYDOG_PULL_KEEP && gpio_is_pulled_down(r.wakePin))) {
      return SLEEPYDOG_ERR_PULLDOWN_ERRATA;
    }
  }
#endif
  return SLEEPYDOG_OK;
#endif // __FREERTOS
}

// Wake pin: set up the pull (gpio_set_pulls keeps the pin's function, so a
// UART RX pin stays a UART pin).  A pin that was never configured (function
// NULL; isolated on RP2350) is made a plain GPIO input first, or it can't wake
static void sdSetupPin(const SleepyDogRPRequest &r) {
  if (r.wakePin < 0) {
    return;
  }
  const uint pin = (uint)r.wakePin;
  if (gpio_get_function(pin) == GPIO_FUNC_NULL) {
    gpio_init(pin);
  }
  switch (r.pull) {
  case SLEEPYDOG_PULL_NONE:
    gpio_disable_pulls(pin);
    break;
  case SLEEPYDOG_PULL_UP:
    gpio_pull_up(pin);
    break;
  case SLEEPYDOG_PULL_DOWN:
    gpio_pull_down(pin);
    break;
  default: // SLEEPYDOG_PULL_KEEP
    break;
  }
  gpio_set_input_enabled(pin, true);
}

// Light sleep: LowPower resumes in place and never parks pins
static int sdLight(const SleepyDogRPRequest &r) {
  const bool pin = r.wakePin >= 0;
  const bool timer = r.durationMs > 0;
  LowPower.keepRunning(r.keepRunning);
  const bool wdt = sdWdtPause();
  int rc;
  if (pin && timer) {
    rc = LowPower.sleepUntilPin((pin_size_t)r.wakePin, r.wakeOn, r.durationMs);
  } else if (pin) {
    rc = LowPower.sleepUntilPin((pin_size_t)r.wakePin, r.wakeOn);
  } else {
    rc = LowPower.sleepFor(r.durationMs);
  }
  LowPower.keepRunning(0);
  sdWdtResume(wdt);
  sdSetResult(rc,
              LowPower.wakeReason() == LowPowerClass::WAKE_PIN
                  ? SLEEPYDOG_WAKE_PIN
                  : SLEEPYDOG_WAKE_TIMER,
              LowPower.lastSleepMs());
  return rc;
}

#if PICO_RP2350
// Deep sleep: SDK power-down (Pstate) with everything but the always-on domain
// off (the core's linker script keeps no persistent RAM).  Wakes by rebooting;
// only returns on error.
static int sdDeepRP2350(const SleepyDogRPRequest &r) {
  const bool pin = r.wakePin >= 0;
  const bool timer = r.durationMs > 0;
  bool edge = false, high = false;
  if (pin) {
    sdMapMode(r.wakeOn, &edge, &high);
  }
  // the AON (powman) timer is the alarm source and measures the sleep
  if (!aon_timer_is_running()) {
    struct timespec ts = {0, 0};
    aon_timer_start(&ts);
  }
  powman_hw->scratch[0] = SD_REC_MAGIC | SD_REC_ARMED | (pin ? SD_REC_PIN : 0) |
                          (timer ? SD_REC_TIMER : 0) |
                          (pin ? ((uint32_t)r.wakePin << SD_REC_PIN_SHIFT) : 0);
  powman_hw->scratch[1] = r.durationMs;
  powman_hw->scratch[2] = (uint32_t)powman_timer_get_ms();

  const bool core1 = setup1 || loop1;
  if (core1) {
    rp2040.idleOtherCore();
  }
  const bool wdt = sdWdtPause();
  int rc;
  if (timer) {
    if (pin) {
      // the SDK's way to wake from either: arm the GPIO, sleep on the timer
      powman_enable_gpio_wakeup(0, (uint32_t)r.wakePin, edge, high);
    }
    rc = low_power_pstate_until_aon_timer(
        aon_timer_make_timeout_time_ms(r.durationMs), nullptr, nullptr);
  } else {
    rc = low_power_pstate_until_gpio_pin_state((uint)r.wakePin, edge, high,
                                               nullptr, nullptr);
  }
  // Only reached if the power-down was refused
  powman_disable_all_wakeups();
  powman_hw->scratch[0] = 0;
  sdWdtResume(wdt);
  if (core1) {
    rp2040.resumeOtherCore();
  }
  if (!rc) {
    rc = PICO_ERROR_GENERIC;
  }
  sdSetResult(rc, SLEEPYDOG_WAKE_NONE, 0);
  return rc;
}

// Runs before setup(): if this boot is the wake from a SleepyDog deep sleep,
// record why and for how long.  The record is accepted only when the last
// reset really was a switched-core power-down, and is cleared once read, so a
// later RUN-pin or watchdog reset is never reported as a deep sleep wake.
static void __attribute__((constructor)) sdCaptureDeepWake(void) {
  const uint32_t rec = powman_hw->scratch[0];
  if ((rec & SD_REC_MAGIC_MASK) != SD_REC_MAGIC || !(rec & SD_REC_ARMED)) {
    return;
  }
  powman_hw->scratch[0] = 0;
  if (!(powman_hw->chip_reset & POWMAN_CHIP_RESET_HAD_SWCORE_PD_BITS)) {
    return;
  }
  // LAST_SWCORE_PWRUP: 1 = pwrup0 (the GPIO wake the SDK arms), 6 = alarm.
  // 0x40 is accepted too in case the field reads one-hot.
  const uint32_t src = powman_hw->last_swcore_pwrup;
  const uint32_t armed = rec & (SD_REC_PIN | SD_REC_TIMER);
  SleepyDogWake why = SLEEPYDOG_WAKE_NONE;
  if (src == 1u) {
    why = SLEEPYDOG_WAKE_DEEP_PIN;
  } else if (src == 6u || src == 0x40u) {
    why = SLEEPYDOG_WAKE_DEEP_TIMER;
  } else if (armed == SD_REC_PIN) {
    why = SLEEPYDOG_WAKE_DEEP_PIN;
  } else if (armed == SD_REC_TIMER) {
    why = SLEEPYDOG_WAKE_DEEP_TIMER;
  }
  if (why != SLEEPYDOG_WAKE_NONE) {
    s_reason = why;
    s_sleptMs = (uint32_t)powman_timer_get_ms() - powman_hw->scratch[2];
  }
}
#endif // PICO_RP2350

/**************************************************************************/
/*!
    @brief  Sleep as described by a request (the engine behind every public
            RP sleep call).
    @param  r
            The request.
    @return 0 after a light sleep, an error code otherwise. A successful
            deep sleep does not return (the chip reboots on wake).
*/
/**************************************************************************/
int sleepydog_rp_sleep(const SleepyDogRPRequest &r) {
  const int rc = sdCheck(r);
  if (rc) {
    sdSetResult(rc, SLEEPYDOG_WAKE_NONE, 0);
    return rc;
  }
  sdSetupPin(r);
  if (!r.deep) {
    return sdLight(r);
  }
#if PICO_RP2350
  return sdDeepRP2350(r);
#else
  return SLEEPYDOG_ERR_UNSUPPORTED;
#endif
}

/**************************************************************************/
/*!
    @brief  Sleep modes and wake sources available on this chip and core.
    @return SLEEPYDOG_CAP_* bits OR-ed together, 0 if sleep is unavailable.
*/
/**************************************************************************/
uint32_t sleepydog_rp_capabilities(void) {
#if defined(__FREERTOS)
  return 0;
#else
  uint32_t caps = SLEEPYDOG_CAP_LIGHT_TIMER | SLEEPYDOG_CAP_LIGHT_PIN |
                  SLEEPYDOG_CAP_LIGHT_PIN_TIMER;
#if PICO_RP2350
  caps |= SLEEPYDOG_CAP_DEEP_TIMER | SLEEPYDOG_CAP_DEEP_PIN |
          SLEEPYDOG_CAP_DEEP_PIN_TIMER | SLEEPYDOG_CAP_DEEP_LOW_POWER;
#endif
  return caps;
#endif
}

/**************************************************************************/
/*!
    @brief  Keep or read up to two 32-bit words that survive a deep sleep
            reboot and other soft resets (not a power cycle or RUN reset).
            RP2350: powman scratch 3..5. RP2040: watchdog scratch 1..3.
    @param  words
            Words to store, or buffer for the words read.
    @param  count
            Number of words, 1 to SLEEPYDOG_PERSIST_WORDS.
    @param  write
            true to store, false to read.
    @return true on success; on read, false if nothing was stored since the
            last power-on (the buffer is then zeroed).
*/
/**************************************************************************/
bool sleepydog_rp_persist(uint32_t *words, uint8_t count, bool write) {
  if (!words || count == 0 || count > SLEEPYDOG_PERSIST_WORDS) {
    return false;
  }
#if PICO_RP2350
  io_rw_32 *mark = &powman_hw->scratch[3];
  io_rw_32 *store = &powman_hw->scratch[4];
#else
  io_rw_32 *mark = &watchdog_hw->scratch[1];
  io_rw_32 *store = &watchdog_hw->scratch[2];
#endif
  if (write) {
    for (uint8_t i = 0; i < SLEEPYDOG_PERSIST_WORDS; i++) {
      store[i] = i < count ? words[i] : 0;
    }
    *mark = SD_PERSIST_MAGIC;
    return true;
  }
  const bool valid = *mark == SD_PERSIST_MAGIC;
  for (uint8_t i = 0; i < count; i++) {
    words[i] = valid ? store[i] : 0;
  }
  return valid;
}

/**************************************************************************/
/*!
    @brief  Watchdog.sleep() on RP: a light timer sleep. Falls back to a plain
            sleep_ms() if LowPower refuses (FreeRTOS), so it never returns
            early.
    @param  ms
            Time to sleep in ms; 0 or less returns at once.
    @return The time slept in ms.
*/
/**************************************************************************/
int sleepydog_rp_watchdog_sleep(int ms) {
  if (ms <= 0) {
    return 0;
  }
  SleepyDogRPRequest r = {false,
                          (uint32_t)ms,
                          -1,
                          LOW,
                          SLEEPYDOG_PULL_KEEP,
                          SLEEPYDOG_RADIO_IGNORE,
                          0,
                          0};
  if (sleepydog_rp_sleep(r) != SLEEPYDOG_OK) {
    sleep_ms((uint32_t)ms);
  }
  return ms;
}

/**************************************************************************/
/*!
    @brief  Dormant that resumes in place, for the legacy RP2350 API
            (goToSleepUntil(ms, true), goToSleepUntilPin()). Watchdog paused.
    @param  pin
            GPIO to wake on, or -1 for a timed dormant.
    @param  mode
            LOW, HIGH, RISING or FALLING (pin wake only).
    @param  ms
            Dormant time in ms (timed dormant only).
    @return 0 on success, error code otherwise.
*/
/**************************************************************************/
int sleepydog_rp_legacy_dormant(int pin, PinStatus mode, uint32_t ms) {
  const bool wdt = sdWdtPause();
  const int rc = pin >= 0 ? LowPower.dormantUntilPin((pin_size_t)pin, mode)
                          : LowPower.dormantFor(ms);
  sdWdtResume(wdt);
  sdSetResult(rc, pin >= 0 ? SLEEPYDOG_WAKE_PIN : SLEEPYDOG_WAKE_TIMER,
              LowPower.lastSleepMs());
  return rc;
}

#else // !SLEEPYDOG_RP_LOWPOWER: core < 6.4.0 or TinyUSB host mode

/**************************************************************************/
/*!
    @brief  Radio state is not tracked without the LowPower engine.
    @return false
*/
/**************************************************************************/
bool sleepydog_rp_radio_on(void) { return false; }

/**************************************************************************/
/*!
    @brief  Sleep is unavailable without the LowPower engine.
    @param  r
            Ignored.
    @return SLEEPYDOG_ERR_UNSUPPORTED
*/
/**************************************************************************/
int sleepydog_rp_sleep(const SleepyDogRPRequest &r) {
  (void)r;
  return SLEEPYDOG_ERR_UNSUPPORTED;
}

/**************************************************************************/
/*!
    @brief  No sleep capabilities without the LowPower engine.
    @return 0
*/
/**************************************************************************/
uint32_t sleepydog_rp_capabilities(void) { return 0; }

/**************************************************************************/
/*!
    @brief  persist() needs the LowPower engine (core >= 6.4.0).
    @param  words
            Ignored.
    @param  count
            Ignored.
    @param  write
            Ignored.
    @return false
*/
/**************************************************************************/
bool sleepydog_rp_persist(uint32_t *words, uint8_t count, bool write) {
  (void)words;
  (void)count;
  (void)write;
  return false;
}

/**************************************************************************/
/*!
    @brief  Watchdog.sleep() without the LowPower engine: busy sleep_ms(),
            as before.
    @param  ms
            Time to sleep in ms; 0 or less returns at once.
    @return The time slept in ms.
*/
/**************************************************************************/
int sleepydog_rp_watchdog_sleep(int ms) {
  if (ms <= 0) {
    return 0;
  }
  sleep_ms((uint32_t)ms);
  return ms;
}

/**************************************************************************/
/*!
    @brief  Legacy dormant needs the LowPower engine.
    @param  pin
            Ignored.
    @param  mode
            Ignored.
    @param  ms
            Ignored.
    @return SLEEPYDOG_ERR_UNSUPPORTED
*/
/**************************************************************************/
int sleepydog_rp_legacy_dormant(int pin, PinStatus mode, uint32_t ms) {
  (void)pin;
  (void)mode;
  (void)ms;
  return SLEEPYDOG_ERR_UNSUPPORTED;
}

#endif // SLEEPYDOG_RP_LOWPOWER

#endif // ARDUINO_ARCH_RP2040
