// Adafruit SleepyDog - deep sleep, pin wake
//
// Deep sleep until GPIO 2 is pulled to GND (FALLING edge, internal pull-up).
// Waking is a reboot: setup() runs again, RAM is lost. wakeReason() tells you
// it was a deep sleep wake, and persist() keeps two 32-bit words across it (a
// boot counter here).
//
// RP2350: chip powered down (about 0.15 mA for the chip), the slept time is
// measured. Optionally pass a timeout (4th argument) to also wake on a timer.
//
// - Pico W / Pico 2 W: power the WiFi chip off first (WiFi.disconnect(),
//   WiFi.powerOff(), as below) or it keeps drawing about 20 mA through the
//   deep sleep. The core powers it up as soon as WiFi or the on-board LED is
//   used. Both calls exist on every board and do nothing without the chip.
//   SleepyDog does not block the sleep if you forget; to make it refuse
//   instead, use Watchdog.setRadioPolicy(SLEEPYDOG_RADIO_MUST_BE_OFF).
// - RP2350: an internal pull-down with a HIGH / RISING wake is refused
//   (erratum RP2350-E9). Use an external pull-down with SLEEPYDOG_PULL_NONE,
//   or a pull-up with LOW / FALLING as here.
// - USB is dropped while asleep and enumerates again after the reboot; flush
//   Serial and give it a few ms before sleeping, or the last lines are lost.
// - A board in deep sleep cannot be reflashed until it wakes or is reset
//   (RUN pin / BOOTSEL).
//
// Needs the arduino-pico core >= 6.4.0 (LowPower library).

#include <Adafruit_SleepyDog.h>
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#endif
#include <WiFi.h> // WiFi.powerOff(): does nothing on boards without WiFi

#define WAKE_PIN 2 // connect to GND to wake

const char *wakeName(int why) {
  switch (why) {
  case SLEEPYDOG_WAKE_TIMER:
    return "timer";
  case SLEEPYDOG_WAKE_PIN:
    return "pin";
  case SLEEPYDOG_WAKE_DEEP_TIMER:
    return "timer (deep sleep reboot)";
  case SLEEPYDOG_WAKE_DEEP_PIN:
    return "pin (deep sleep reboot)";
  default:
    return "none (power-on or reset)";
  }
}

// USB serial only starts a transfer on flush(): give it a few ms to finish
void flushSerial() {
  Serial.flush();
  for (uint32_t t0 = millis(); millis() - t0 < 5;) {
    yield();
  }
}

void setup() {
  // read the wake reason first: it is valid from the start of setup()
  uint32_t sleptMs = 0;
  int why = Watchdog.wakeReason(&sleptMs);

  uint32_t boots = 0;
  Watchdog.persist(&boots, 1, false); // false (boots = 0) after power-on
  boots++;
  Watchdog.persist(&boots, 1, true);

  Serial.begin(115200);
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 3000;) {
    delay(10);
  }
  Serial.println("Adafruit SleepyDog - deep sleep, pin wake");
  Serial.print("Boot #");
  Serial.print(boots);
  Serial.print(", wake reason: ");
  Serial.print(wakeName(why));
  if (why == SLEEPYDOG_WAKE_DEEP_TIMER || why == SLEEPYDOG_WAKE_DEEP_PIN) {
    Serial.print(", slept ");
    if (sleptMs == SLEEPYDOG_SLEPT_UNKNOWN) {
      Serial.print("(unknown)");
    } else {
      Serial.print(sleptMs);
      Serial.print(" ms");
    }
  }
  Serial.println();

  if (!(Watchdog.sleepCapabilities() & SLEEPYDOG_CAP_DEEP_PIN)) {
    Serial.println("Deep sleep with a pin wake is not available on this "
                   "chip/core");
  }
}

void loop() {
  Serial.println("Awake for 5 seconds...");
  delay(5000);

  // Pico W / Pico 2 W: a powered WiFi chip keeps drawing current through the
  // sleep, and the on-board LED powers it up too. Close sockets / MQTT first
  // in a real sketch. Both calls do nothing on boards without the chip.
  WiFi.disconnect();
  WiFi.powerOff();

  Serial.println("Deep sleep until GPIO 2 goes LOW, the board reboots on wake");
  flushSerial();
  // does not return on success
  int rc = Watchdog.deepSleepUntilPin(WAKE_PIN, FALLING, SLEEPYDOG_PULL_UP);

  Serial.print("Deep sleep failed: ");
  Serial.print(rc);
  Serial.print(" ");
  Serial.println(Watchdog.sleepErrorString(rc));
}
