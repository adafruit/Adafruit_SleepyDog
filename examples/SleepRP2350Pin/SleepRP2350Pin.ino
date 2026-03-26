// Adafruit Watchdog Library - RP2350 Pin/GPIO Sleep Example
//
// Simple example for using the Adafruit Watchdog Library to put the RP2350 to
// sleep until a GPIO pin changes state.
//
// Brent Rubell for Adafruit Industries
//
// NOTE: After uploading, physically unplug and replug the USB cable to
// reset the AON timer correctly before running.

#include <Adafruit_SleepyDog.h>
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#else
#include "USB.h"
#endif
#define WAKE_PIN 2 // GPIO pin to wake on, change as needed for your use case

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  // while (!Serial);
  Serial.println("Adafruit Watchdog Library - RP2350 Sleep Pin/GPIO Demo!");
  Serial.println();
}

void loop() {
  Serial.println("Going to sleep in 5 seconds...");
  Serial.println("To wake up, connect GPIO pin 2 to 3.3V (for HIGH wake).");
  delay(5000);

  // Enter Sleep mode for 5 seconds
  digitalWrite(LED_BUILTIN, LOW);

  // Sleep until GPIO WAKE_PIN goes HIGH on a rising edge
  Watchdog.goToSleepUntilPin(WAKE_PIN, true, true);

  // Sleep until GPIO WAKE_PIN goes HIGH on a falling edge
  // Watchdog.goToSleepUntilPin(WAKE_PIN, false, true);

  // Sleep until GPIO WAKE_PIN goes LOW on a rising edge
  // Watchdog.goToSleepUntilPin(WAKE_PIN, true, false);

  // Sleep until GPIO WAKE_PIN goes LOW on a falling edge
  // Watchdog.goToSleepUntilPin(WAKE_PIN, false, false);

  // Re-enable clocks, generators, USB and resume execution
  // NOTE: This MUST be called to properly resume from sleep!
  Watchdog.resumeFromSleep();
  #ifndef USE_TINYUSB
  // Re-enable USB after sleep. Required for Windows where USB
  // does not re-enumerate automatically. On macOS and Linux, USB is
  // maintained across sleep cycles. If you use an external serial terminal
  // (e.g. screen, minicom), comment out USB.disconnect() and USB.connect()
  // to prevent the port from disconnecting on each wake cycle.
  USB.disconnect();
  delay(500); // Give host time to register disconnect before reconnecting
  USB.connect();
  #endif

  // NOTE: We can not track sleep duration when waking from a pin because we use
  // the crystal oscillator as the dormant clock source, so the AON timer we'd
  // use to track sleep duration is turned off. If you need to track sleep
  // duration, see the SleepRP2350Timer example which uses the AON timer to wake
  // from sleep.
  Serial.println("I'm awake now!");
}
