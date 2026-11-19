// Adafruit SleepyDog - Pico W / Pico 2 W: WiFi off, sleep, WiFi back on
//
// SleepyDog never switches the WiFi chip itself; the sketch does:
//   close sockets / MQTT  ->  WiFi.powerOff()  ->  sleep  -> WiFi.powerOn(true)
//
// Is it worth powering WiFi off? (arduino-pico docs/wifi.rst, "Powering the
// WiFi Chip Off", measured on a Pico 2 W):
// - light sleep with the station connected: about 51 mA; after WiFi.end()
//   about 19 mA; after WiFi.powerOff() about 4.4 mA.
// - Re-joining after powerOn() takes about 7 s (6-18 s measured), so powering
//   off only pays for sleeps longer than about 9 s. For shorter sleeps keep
//   the station connected: light sleep keeps it alive (the default radio
//   policy never blocks a sleep).
// - powerOn()'s default 15 s timeout can be too short for a slow access
//   point; 30 s is used here.
// - Sockets, clients and servers do not survive powerOff(): create them again
//   after powerOn(). A static IP set with WiFi.config() is kept.
// - Bluetooth stops too and must be started again after powerOn().
//
// The sleep uses SLEEPYDOG_RADIO_MUST_BE_OFF, so if WiFi were still powered
// it would return SLEEPYDOG_ERR_RADIO_ON instead of sleeping.
//
// Needs the arduino-pico core >= 6.4.0 and a Pico W or Pico 2 W.

#include <Adafruit_SleepyDog.h>
#include <WiFi.h>
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#endif

#ifndef WIFI_SSID
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
#endif

const uint32_t SLEEP_MS = 60000;       // well above the ~9 s break-even
const uint32_t REJOIN_TIMEOUT = 30000; // ms

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
  Serial.begin(115200);
  for (uint32_t t0 = millis(); !Serial && millis() - t0 < 3000;) {
    delay(10);
  }
  Serial.println("Adafruit SleepyDog - WiFi powerOff / sleep / powerOn");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (uint32_t t0 = millis();
       WiFi.status() != WL_CONNECTED && millis() - t0 < REJOIN_TIMEOUT;) {
    delay(100);
  }
  Serial.print("WiFi ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "connected" : "NOT connected");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // ... do the network work here (HTTP, MQTT publish, ...) ...
    Serial.print("IP ");
    Serial.print(WiFi.localIP());
    Serial.print(", RSSI ");
    Serial.println(WiFi.RSSI());
  }

  // Close sockets / MQTT here: they do not survive powerOff()
  WiFi.powerOff();

  SleepyDogConfig cfg;
  cfg.durationMs = SLEEP_MS;
  cfg.radio = SLEEPYDOG_RADIO_MUST_BE_OFF;

  Serial.print("WiFi off, light sleep for ");
  Serial.print(SLEEP_MS / 1000);
  Serial.println(" s");
  Serial.flush();
  int rc = Watchdog.sleep(cfg);
  if (rc != SLEEPYDOG_OK) {
    Serial.print("Sleep failed: ");
    Serial.println(Watchdog.sleepErrorString(rc));
  } else {
    uint32_t sleptMs = 0;
    int why = Watchdog.wakeReason(&sleptMs);
    Serial.print("Woke up, reason: ");
    Serial.print(wakeName(why));
    Serial.print(", slept ");
    Serial.print(sleptMs);
    Serial.println(" ms");
  }

  uint32_t t0 = millis();
  bool joined = WiFi.powerOn(true, REJOIN_TIMEOUT);
  Serial.print("WiFi powerOn: ");
  Serial.print(joined ? "joined" : "not joined (yet)");
  Serial.print(" after ");
  Serial.print(millis() - t0);
  Serial.println(" ms");
}
