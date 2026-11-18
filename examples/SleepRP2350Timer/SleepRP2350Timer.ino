// Adafruit Watchdog Library - RP2350 Timer Sleep Example
//
// Legacy RP2350 sleep API: Watchdog.goToSleepUntil(ms) is a light sleep (the
// sketch resumes where it stopped), Watchdog.goToSleepUntil(ms, true) a
// dormant sleep (lower power, USB drops and re-enumerates).
//
// With arduino-pico >= 6.4.0 both run on the core's LowPower library: the
// clocks come back by themselves (resumeFromSleep() is kept only for
// compatibility), a Pico 2 W with WiFi connected should wake up again
// (adafruit/Adafruit_SleepyDog#72, not yet confirmed on hardware), and a
// watchdog enabled with Watchdog.enable() is paused for the sleep and re-armed
// after.
// For new sketches (both RP2040 and RP2350, deep sleep, pin wake, wake reason)
// see the RPSleep* examples.
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

static volatile bool awake;

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

  awake = false;
  digitalWrite(LED_BUILTIN, LOW);
  Serial.flush();

#if !defined(USE_TINYUSB) && !defined(NO_USB)
  // Disconnect USB before sleeping so the host sees a clean unplug instead of
  // a hung device, and (light sleep) the USB clocks can stop too. The
  // Adafruit TinyUSB stack handles this itself.
  USB.disconnect();
#endif

  // Light sleep for 5 seconds
  Watchdog.goToSleepUntil(5000);
  // Uncomment the line below (and comment the line above) to enter Dormant
  // instead of light sleep
  // Watchdog.goToSleepUntil(5000, true);

  // The wake callback has run by now
  while (!awake) {
    delay(1);
  }

  // NOTE: on cores older than 6.4.0 this MUST be called to resume from sleep
  Watchdog.resumeFromSleep();

#if !defined(USE_TINYUSB) && !defined(NO_USB)
  // Reconnect USB: the host sees a normal plug-in and enumerates again
  USB.connect();
#endif
  // Give the host a moment to open the port again, or the first lines are lost
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 2000;) {
    delay(10);
  }

  Serial.println("I'm awake now! (woken by the timer)");
  Serial.print("Slept for approximately ");
  long sleep_duration = Watchdog.getSleepDuration();
  Serial.print(sleep_duration);
  Serial.println(" milliseconds.");
}
