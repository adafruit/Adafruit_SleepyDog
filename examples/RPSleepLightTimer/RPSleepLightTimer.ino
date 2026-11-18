// Adafruit SleepyDog - RP2040 / RP2350 light sleep, timer wake
//
// Light sleep gates the clocks and stops the PLLs. The sketch resumes right
// after Watchdog.lightSleep(ms): RAM, pin states, USB (while a host uses it)
// and a connected WiFi station are kept. Typical board current: RP2040 0.8-1.5
// mA (3.1 mA while a USB host is using the port; Adafruit TinyUSB builds always
// count as "in use"), RP2350 1.6-4.7 mA.
//
// Pico W / Pico 2 W: the on-board LED is wired to the WiFi chip, so blinking it
// keeps that chip powered (about 15 mA more while asleep on a Pico 2 W). See
// RPSleepWiFi for powering it off.
//
// The hardware watchdog is enabled with an 8 s timeout and the sketch sleeps
// for 10 s: SleepyDog pauses the watchdog for the sleep and re-arms it with the
// same timeout afterwards, so the board is not reset.
//
// Needs the arduino-pico core >= 6.4.0 (LowPower library). Works on Pico,
// Pico W, Pico 2, Pico 2 W and other RP2040 / RP2350 boards.

#include <Adafruit_SleepyDog.h>
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#endif

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
    return "none";
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 3000;) {
    delay(10);
  }
  Serial.println("Adafruit SleepyDog - RP light sleep, timer wake");

  if (!(Watchdog.sleepCapabilities() & SLEEPYDOG_CAP_LIGHT_TIMER)) {
    Serial.println("Light sleep is not available: needs arduino-pico >= "
                   "6.4.0 (and not the TinyUSB host stack)");
  }

  int countdownMS = Watchdog.enable(8000);
  Serial.print("Watchdog enabled, timeout ");
  Serial.print(countdownMS);
  Serial.println(" ms");
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Awake for 2 seconds...");
  for (int i = 0; i < 2; i++) {
    delay(1000);
    Watchdog.reset();
  }
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("Light sleep for 10 seconds");
  Serial.flush();
  // 10 s: longer than the watchdog timeout, on purpose
  int rc = Watchdog.lightSleep(10000);

  if (rc != SLEEPYDOG_OK) {
    Serial.print("Sleep failed: ");
    Serial.print(rc);
    Serial.print(" ");
    Serial.println(Watchdog.sleepErrorString(rc));
    return;
  }
  uint32_t sleptMs = 0;
  int why = Watchdog.wakeReason(&sleptMs);
  Serial.print("Woke up, reason: ");
  Serial.print(wakeName(why));
  Serial.print(", slept ");
  Serial.print(sleptMs);
  Serial.println(" ms");
  Watchdog.reset();
}
