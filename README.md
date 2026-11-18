# Adafruit SleepyDog Arduino Library [![Build Status](https://github.com/adafruit/Adafruit_SleepyDog/workflows/Arduino%20Library%20CI/badge.svg)](https://github.com/adafruit/Adafruit_SleepyDog/actions)[![Documentation](https://github.com/adafruit/ci-arduino/blob/master/assets/doxygen_badge.svg)](http://adafruit.github.io/Adafruit_SleepyDog/html/index.html)

Arduino library to use the watchdog timer for system reset. Also provides  simple API access for entering/exiting from low power sleep modes.

## Supported Platforms
| Support Level | Platform / Board Family | MCU(s) | Notes / Caveats |
|--------------|------------------------|--------|-----------------|
| **Full** | Raspberry Pi Pico | RP2040, RP2350 | Low-power sleep (light, deep) needs arduino-pico >= 6.4.0, see below. |
| **Full** | ESP32 Family | ESP32, ESP32-S2, ESP32-S3 | — |
| **Limited** | ESP8266 | ESP8266 | Software and hardware watchdog timers are fixed-interval and not programmable. See `utility/WatchdogESP8266.cpp`. |
| **Full** | Arduino Uno / ATmega328P Boards | ATmega328P | — |
| **Full** | Arduino Mega | ATmega2560, ATmega1280 | — |
| **Full** | Arduino Zero / Adafruit Feather M0 | ATSAMD21 | — |
| **Full** | Adafruit Feather M4 | ATSAMD51 | — |
| **Full** | Arduino Leonardo / 32u4 Boards | ATmega32u4 | USB Serial is disabled during sleep. Logic continues running but Serial appears frozen. Restore after wake with `USBDevice.attach();` and reconnect from host. |
| **Full** | ATtiny Series | ATtiny24/44/84, ATtiny25/45/85 | — |
| **Partial** | Teensy 3.x / LC | Teensy 3.x, LC | Watchdog supported; sleep not supported. |


## Low-power sleep on RP2040 / RP2350 (Pico, Pico W, Pico 2, Pico 2 W)

Needs the [arduino-pico](https://github.com/earlephilhower/arduino-pico) core **6.4.0 or newer**; SleepyDog builds on its `LowPower` library. On older cores, in Adafruit TinyUSB **host**-mode builds, and with FreeRTOS, the sleep methods return an error and `sleepCapabilities()` returns 0. `Watchdog.enable()`, `reset()`, `disable()` and `sleep(ms)` work everywhere.

| | RP2040 (Pico, Pico W) | RP2350 (Pico 2, Pico 2 W) |
|---|---|---|
| **Light sleep**, timer and/or pin | yes: board about 0.8–1.5 mA (3.1 mA while a USB host uses the port) | yes: board about 1.6–4.7 mA |
| **Deep sleep**, timer | not supported (needs an external 32 kHz clock): `SLEEPYDOG_ERR_UNSUPPORTED` | yes: chip powered down (about 0.15 mA), reboots on wake |
| **Deep sleep**, pin | not supported: `SLEEPYDOG_ERR_UNSUPPORTED` | yes: powered down, reboots on wake |
| **Deep sleep**, pin or timer | no | yes |

* **Light sleep** gates the clocks and stops the PLLs. The sketch resumes right after the call. RAM and pin states are kept, and so are USB (while a host uses it) and a connected WiFi station. Pins are never parked.
* **Deep sleep** is a reboot on wake: `setup()` runs again. Only the wake reason and two `persist()` words survive. Anything bigger belongs in flash (EEPROM / LittleFS).

```cpp
#include <Adafruit_SleepyDog.h>

int rc = Watchdog.lightSleep(10000);                 // timer wake
rc = Watchdog.lightSleepUntilPin(2, FALLING, SLEEPYDOG_PULL_UP, 10000); // pin or timer
if (rc != SLEEPYDOG_OK) Serial.println(Watchdog.sleepErrorString(rc));

uint32_t sleptMs;
int why = Watchdog.wakeReason(&sleptMs); // SLEEPYDOG_WAKE_TIMER / _PIN ...

Watchdog.deepSleep(10000);                           // does not return on success
Watchdog.deepSleepUntilPin(2, FALLING, SLEEPYDOG_PULL_UP);
```

| method | meaning |
|---|---|
| `lightSleep(ms)` | light sleep, timer wake |
| `lightSleepUntilPin(pin, mode, pull = SLEEPYDOG_PULL_KEEP, timeoutMs = 0)` | light sleep until the pin, or the timeout (0 = none) |
| `deepSleep(ms)` | deep sleep, timer wake (at least 10 ms) |
| `deepSleepUntilPin(pin, mode, pull = SLEEPYDOG_PULL_KEEP, timeoutMs = 0)` | deep sleep until the pin, or the timeout (RP2350 only) |

* `mode` is `LOW` / `HIGH` (a level: returns at once if the pin is already there) or `RISING` / `FALLING` (an edge).
* `pull` is `SLEEPYDOG_PULL_KEEP` (leave as configured), `_NONE`, `_UP` or `_DOWN`.

Setters for options that apply to every following sleep:

| setter | default | meaning |
|---|---|---|
| `setRadioPolicy(p)` | `SLEEPYDOG_RADIO_IGNORE` | see *WiFi* below |
| `keepPins(mask)` | `0` | RP2040 deep sleep only: GPIO bit mask left untouched while dormant |
| `keepRunning(mask)` | `0` | light sleep only: `SLEEPYDOG_KEEP_PWM`, `_PIO`, `_UART0`, `_UART1`, `_ADC`, `_DMA` (keeps PLL_SYS on, about 4 mA more) |

Other calls:

* `sleepCapabilities()` returns `SLEEPYDOG_CAP_*` bits: `LIGHT_TIMER`, `LIGHT_PIN`, `LIGHT_PIN_TIMER`, `DEEP_TIMER`, `DEEP_PIN`, `DEEP_PIN_TIMER` and `DEEP_LOW_POWER`. `DEEP_LOW_POWER` means deep sleep draws much less than light sleep, so it is worth the reboot.
* `wakeReason(&sleptMs)` returns one of these:
  * `SLEEPYDOG_WAKE_NONE`: power-on, reset, or the last sleep failed;
  * `_TIMER` / `_PIN`: after a light sleep;
  * `_DEEP_TIMER` / `_DEEP_PIN`: from the start of `setup()` after a deep sleep until the next sleep.

  `sleptMs` is measured. After an RP2350 deep sleep it comes from the always-on timer, including the boot time. It is `SLEEPYDOG_SLEPT_UNKNOWN` when no clock ran.
* `persist(words, count, write)` stores or reads 1–2 `uint32_t` words. They survive deep sleep, watchdog resets and reboots, but not a power cycle or the RUN pin. A read returns `false` when nothing has been stored since power-on.
* `radioIsOn()` tells whether the on-board CYW43 WiFi chip is powered: Pico W, Pico 2 W, and other boards whose core definition has one, such as Pimoroni's RM2 boards. It does not see AirLift / ESP32 coprocessors.
* `sleepErrorString(rc)` describes a return code. A return code is 0, a `SLEEPYDOG_ERR_*` code (−100 and below), or a Pico SDK `PICO_ERROR_*` code passed through, e.g. `PICO_ERROR_NOT_PERMITTED` with FreeRTOS.

**Watchdog.** `LowPower` never touches the hardware watchdog. SleepyDog stops a watchdog started with `Watchdog.enable()` for the sleep and re-arms it with the same timeout after a light sleep, so sleeping longer than the timeout is fine. A deep sleep wakes with the watchdog off. If the watchdog was enabled outside SleepyDog, it is re-armed at the hardware maximum (8.3 s RP2040, 16.7 s RP2350). `Watchdog.sleep(ms)` itself is a light sleep with the same handling.

**Wake pin.**
* The `pull` argument sets the pull without changing the pin's function. A UART RX pin can wake the board and stays a UART pin; add `SLEEPYDOG_KEEP_UART0` to receive the first byte intact. A pin that was never configured is made a GPIO input.
* RP2350 erratum **E9**: an internal pull-down with a `HIGH` / `RISING` wake is unreliable, because the pin latches near 2 V. SleepyDog refuses it with `SLEEPYDOG_ERR_PULLDOWN_ERRATA`; that includes an unconfigured RP2350 pin, which resets with its pull-down on. Use an external pull-down with `SLEEPYDOG_PULL_NONE`, or a pull-up with `LOW` / `FALLING`.

**WiFi (Pico W, Pico 2 W and other boards with an on-board CYW43).** SleepyDog never switches the WiFi chip and, by default, never refuses to sleep because of it. A powered chip keeps drawing current through the sleep (about 20 mA), so for the lowest current the sketch does: close sockets / MQTT, `WiFi.disconnect()`, `WiFi.powerOff()`, sleep. After a light sleep, `WiFi.powerOn(true)` re-joins; after a deep sleep the reboot starts WiFi from scratch.
* The core powers the chip up as soon as WiFi **or the on-board LED** is used (the LED is wired to the WiFi chip), so a blinky sketch has it powered too. That doesn't block a sleep; it only costs current.
* `WiFi.powerOff()` and `powerOn()` exist on every board and do nothing without a CYW43, so the same sketch builds everywhere. The deep sleep examples call them unconditionally.
* `setRadioPolicy()`: `SLEEPYDOG_RADIO_IGNORE` (the default) never refuses. `SLEEPYDOG_RADIO_MUST_BE_OFF` makes a sleep return `SLEEPYDOG_ERR_RADIO_ON` while the chip is powered, for sketches that want to be sure it is off.
* AirLift / ESP32 WiFi coprocessors are not seen by SleepyDog. That includes the Fruit Jam's ESP32-C6 and AirLift breakouts or FeatherWings. If one should not draw current while asleep, end WiFi and hold it in reset yourself. On the Fruit Jam, `ESP32_RESETN` is defined as `PIN_PERIPHERAL_RESET`.

From the arduino-pico [WiFi docs](https://github.com/earlephilhower/arduino-pico/blob/master/docs/wifi.rst#powering-the-wifi-chip-off-pico-w--pico-2-w), measured on a Pico 2 W:
* Light sleep draws about 51 mA connected, about 19 mA after `WiFi.end()`, and about 4.4 mA after `WiFi.powerOff()`.
* Re-joining after `powerOn()` takes about 7 s (6–18 s seen), so powering off only pays for sleeps longer than about 9 s. The default 15 s `powerOn()` timeout can be too short for a slow access point.
* Sockets and servers do not survive `powerOff()`; a static IP is kept.
* Bluetooth stops too and must be restarted.

**USB and Serial.**
* Light sleep keeps the USB connection while a host uses it.
* Deep sleep and dormant drop USB, and the host re-enumerates about 0.6 s after the wake. Output printed just before the sleep, or in the first second after the wake, can be lost. Before a deep sleep call `Serial.flush()` and `yield()` for about 5 ms.
* A board in deep sleep cannot be reflashed until it wakes or is reset (RUN / BOOTSEL).

**Adafruit TinyUSB.** Repeated dormant cycles that resume in place (the legacy `goToSleepUntil(ms, true)` and `goToSleepUntilPin()`) need Adafruit TinyUSB **3.8.0** or newer; 3.7.7, bundled with core 6.4.0, panics on the 3rd wake. Deep sleep reboots after every wake, so it is not affected.

**Feather RP2040 USB Host.** After a dormant wake, a device on the PIO-USB host port is only detected again if the sketch switches the USB-A 5 V supply (GPIO 18) off for about 300 ms.

**Not handled by SleepyDog:** Serial flushing before deep sleep, re-initialising sensors / I2C after a deep sleep reboot, and state larger than the two `persist()` words.

**Examples:**
* `RPSleepLightTimer`, `RPSleepLightPin`: light sleep, timer wake and pin wake with a timeout;
* `RPSleepDeepTimer`, `RPSleepDeepPin`: deep sleep;
* `RPSleepWiFi`: WiFi off, sleep, WiFi back on;
* `SleepRP2350Timer`, `SleepRP2350Pin`: the legacy RP2350 API (`goToSleepUntil()`, `goToSleepUntilPin()`), which still works and runs on `LowPower` from core 6.4.0. That should also let a Pico 2 W with WiFi connected wake up again (adafruit/Adafruit_SleepyDog#72), not yet confirmed on hardware.
