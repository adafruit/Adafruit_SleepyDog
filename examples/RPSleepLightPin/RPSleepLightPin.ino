// Adafruit SleepyDog - RP2040 / RP2350 light sleep, pin wake with a timeout
//
// Light sleep until GPIO 2 is pulled to GND (FALLING edge, internal pull-up)
// or 30 seconds have passed, whichever comes first. The sketch resumes right
// after Watchdog.lightSleepUntilPin() and wakeReason() says which one it was.
//
// Pico W / Pico 2 W: the on-board LED is wired to the WiFi chip, so blinking it
// keeps that chip powered (about 15 mA more while asleep on a Pico 2 W). See
// RPSleepWiFi for powering it off.
//
// Wake pin notes:
// - mode LOW / HIGH is a level: if the pin is already at that level the
//   sleep ends at once. RISING / FALLING wait for an edge.
// - the pull is applied before sleeping (SLEEPYDOG_PULL_KEEP leaves the pin
//   as you configured it). It does not change the pin's function, so a UART
//   RX pin can wake the board and stays a UART pin; call
//   Watchdog.keepRunning(SLEEPYDOG_KEEP_UART0) to receive the first byte
//   intact.
// - RP2350: an internal pull-down with a HIGH / RISING wake is refused
//   (erratum RP2350-E9, SLEEPYDOG_ERR_PULLDOWN_ERRATA). Use an external
//   pull-down (e.g. 10k to GND) with SLEEPYDOG_PULL_NONE, or a pull-up with
//   LOW / FALLING as here.
//
// Needs the arduino-pico core >= 6.4.0 (LowPower library).

#include <Adafruit_SleepyDog.h>
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#endif

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
    return "none";
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 3000;) {
    delay(10);
  }
  Serial.println("Adafruit SleepyDog - RP light sleep, pin wake + timeout");
  if (!(Watchdog.sleepCapabilities() & SLEEPYDOG_CAP_LIGHT_PIN_TIMER)) {
    Serial.println("Light sleep is not available: needs arduino-pico >= "
                   "6.4.0 (and not the TinyUSB host stack)");
  }
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Awake for 2 seconds...");
  delay(2000);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println("Light sleep until GPIO 2 goes LOW, or 30 seconds");
  Serial.flush();
  // last argument: timeout in ms, 0 = wait for the pin only
  int rc =
      Watchdog.lightSleepUntilPin(WAKE_PIN, FALLING, SLEEPYDOG_PULL_UP, 30000);

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
}
