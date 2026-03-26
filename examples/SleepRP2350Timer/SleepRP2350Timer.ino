// Adafruit Watchdog Library - RP2350 Timer Sleep Example
//
// Simple example for using the Adafruit Watchdog Library to put the RP2350 to
// sleep until the AON timer expires. NOTE: This example exposes both the Sleep
// State (6.5.2 in RP2350 Datasheet) and the Dormant State (6.5.3 in RP2350
// Datasheet) APIs.
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

static bool awake;

// This function will be called when the device wakes from sleep
// You can add any custom behavior you want here
void cbWake() {
  // Show we're awake again
  digitalWrite(LED_BUILTIN, HIGH);
  awake = true;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  Serial.begin(115200);
  // while(!Serial);
  Serial.println("Adafruit Watchdog Library - RP2350 Sleep Timer Demo!");
  Serial.println();

  // When the device wakes back up, we'll call this callback function
  // to indicate that we've resumed from sleep.
  Watchdog.setWakeCb(cbWake);
}

void loop() {
  Serial.println("Going to sleep in 5 seconds...");
  delay(5000);

  // Enter Sleep mode for 5 seconds
  awake = false;
  digitalWrite(LED_BUILTIN, LOW);
  // Enter sleep state (6.5.2 in RP2350 Datasheet)
  Watchdog.goToSleepUntil(5000);
  // Uncomment the line below (and comment the line above) to enter Dormant
  // State (6.5.3 in RP2350 Datasheet) instead of the Sleep State above
  // Watchdog.goToSleepUntil(5000, true);

  // Make sure we don't wake
  while (!awake) {
    Serial.println("Should be sleeping here...");
  }

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

  Serial.println("I'm awake now!");
  Serial.print("Slept for approximately ");
  long sleep_duration = Watchdog.getSleepDuration();
  Serial.print(sleep_duration);
  Serial.println(" milliseconds.");
}
