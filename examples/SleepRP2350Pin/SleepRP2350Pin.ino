// Adafruit Watchdog Library - RP2350 Pin/GPIO Sleep Example
//
// Legacy RP2350 sleep API: Watchdog.goToSleepUntilPin(pin, edge, high) puts
// the RP2350 into dormant until a GPIO pin changes state (USB drops and
// re-enumerates after the wake).
//
// With arduino-pico >= 6.4.0 this runs on the core's LowPower library
// (LowPower.dormantUntilPin()); resumeFromSleep() is kept only for
// compatibility. For new sketches (both RP2040 and RP2350, deep sleep, a
// timeout, wake reason) see the RPSleep* examples.
//
// The wake pin uses the internal pull-up and wakes on a FALLING edge: connect
// GPIO 2 to GND to wake. A HIGH/RISING wake with the internal pull-down is
// unreliable on RP2350 (erratum RP2350-E9); use an external pull-down
// (e.g. 10k to GND) if you need to wake on a high level.
//
// Repeated dormant cycles in an Adafruit TinyUSB build need Adafruit TinyUSB
// >= 3.8.0 (3.7.7, bundled with core 6.4.0, panics on the 3rd wake).
//
// Brent Rubell for Adafruit Industries

#include <Adafruit_SleepyDog.h>
#if defined(USE_TINYUSB)
#include <Adafruit_TinyUSB.h>
#elif !defined(NO_USB)
#include "USB.h"
#endif

#define WAKE_PIN 2 // GPIO pin to wake on, change as needed for your use case

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  pinMode(WAKE_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  // while (!Serial);
  Serial.println("Adafruit Watchdog Library - RP2350 Sleep Pin/GPIO Demo!");
  Serial.println();
}

void loop() {
  Serial.println("Going to sleep in 5 seconds...");
  Serial.println("To wake up, connect GPIO pin 2 to GND (FALLING edge).");
  delay(5000);

  digitalWrite(LED_BUILTIN, LOW);
  Serial.flush();

#if !defined(USE_TINYUSB) && !defined(NO_USB)
  // Disconnect USB before sleeping so the host sees a clean unplug instead of
  // a hung device. The Adafruit TinyUSB stack handles this itself.
  USB.disconnect();
#endif

  // Sleep until GPIO WAKE_PIN goes LOW on a falling edge (edge=true,
  // high=false)
  Watchdog.goToSleepUntilPin(WAKE_PIN, true, false);

  // Other conditions: (true, true) rising edge, (false, true) high level,
  // (false, false) low level.

  // NOTE: on cores older than 6.4.0 this MUST be called to resume from sleep
  Watchdog.resumeFromSleep();
  digitalWrite(LED_BUILTIN, HIGH);

#if !defined(USE_TINYUSB) && !defined(NO_USB)
  // Reconnect USB: the host sees a normal plug-in and enumerates again
  USB.connect();
#endif
  // Give the host a moment to open the port again, or the first lines are lost
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 2000;) {
    delay(10);
  }

  // No clock runs while dormant, so a pin wake has no measured duration:
  // getSleepDuration() returns -1 then.
  Serial.println("I'm awake now! (woken by the pin)");
  Serial.print("Sleep duration (ms, -1 = unknown): ");
  Serial.println(Watchdog.getSleepDuration());
}
