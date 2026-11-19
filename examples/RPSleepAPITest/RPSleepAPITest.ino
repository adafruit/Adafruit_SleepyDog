// Adafruit SleepyDog - RP sleep API compile test (config struct API)
//
// NOT a demo: CI builds this sketch on every RP2040 / RP2350 target. It uses
// every public RP sleep method, field and enum value, so a build fails if any
// of them changes or disappears, and it refuses to build if the LowPower
// engine is not compiled in. The calls sit behind a flag that is never set, so
// running it does nothing harmful.

#include <Adafruit_SleepyDog.h>
#ifdef USE_TINYUSB
#include <Adafruit_TinyUSB.h>
#endif

#if !SLEEPYDOG_RP_LOWPOWER
#error "SleepyDog RP sleep engine not built (needs arduino-pico >= 6.4.0)"
#endif

static_assert(SLEEPYDOG_PERSIST_WORDS == 2, "two persist words");
static_assert(SLEEPYDOG_SLEPT_UNKNOWN == 0xFFFFFFFFu, "unknown slept time");

volatile bool runApi = false; // never set: the calls are compiled and linked
volatile int sink;

void onWake() { sink = 1; }

void apiTest() {
  SleepyDogConfig cfg;
  cfg.mode = SLEEPYDOG_LIGHT;
  cfg.mode = SLEEPYDOG_DEEP;
  cfg.durationMs = 1000;
  cfg.wakePin = 2;
  cfg.wakeOn = LOW;
  cfg.wakeOn = HIGH;
  cfg.wakeOn = RISING;
  cfg.wakeOn = FALLING;
  cfg.wakePull = SLEEPYDOG_PULL_KEEP;
  cfg.wakePull = SLEEPYDOG_PULL_NONE;
  cfg.wakePull = SLEEPYDOG_PULL_UP;
  cfg.wakePull = SLEEPYDOG_PULL_DOWN;
  cfg.radio = SLEEPYDOG_RADIO_IGNORE;
  cfg.radio = SLEEPYDOG_RADIO_MUST_BE_OFF;
  cfg.keepPins = 1ull << 3;
  cfg.keepRunning = SLEEPYDOG_KEEP_PWM | SLEEPYDOG_KEEP_PIO |
                    SLEEPYDOG_KEEP_UART0 | SLEEPYDOG_KEEP_UART1 |
                    SLEEPYDOG_KEEP_ADC | SLEEPYDOG_KEEP_DMA;
  int rc = Watchdog.sleep(cfg);
  const SleepyDogConfig defaults{};
  rc |= Watchdog.sleep(defaults);

  uint32_t caps = Watchdog.sleepCapabilities();
  const uint32_t allCaps = SLEEPYDOG_CAP_LIGHT_TIMER | SLEEPYDOG_CAP_LIGHT_PIN |
                           SLEEPYDOG_CAP_LIGHT_PIN_TIMER |
                           SLEEPYDOG_CAP_DEEP_TIMER | SLEEPYDOG_CAP_DEEP_PIN |
                           SLEEPYDOG_CAP_DEEP_PIN_TIMER |
                           SLEEPYDOG_CAP_DEEP_LOW_POWER;
  caps &= allCaps;

  const int errors[] = {SLEEPYDOG_OK,
                        SLEEPYDOG_ERR_UNSUPPORTED,
                        SLEEPYDOG_ERR_NO_WAKE_SOURCE,
                        SLEEPYDOG_ERR_BAD_PIN,
                        SLEEPYDOG_ERR_BAD_WAKE_MODE,
                        SLEEPYDOG_ERR_RADIO_ON,
                        SLEEPYDOG_ERR_PULLDOWN_ERRATA,
                        SLEEPYDOG_ERR_TOO_SHORT};
  for (int e : errors) {
    sink = Watchdog.sleepErrorString(e)[0];
  }

  uint32_t sleptMs = 0;
  int why = Watchdog.wakeReason(&sleptMs);
  why = Watchdog.wakeReason();
  switch (why) {
  case SLEEPYDOG_WAKE_NONE:
  case SLEEPYDOG_WAKE_TIMER:
  case SLEEPYDOG_WAKE_PIN:
  case SLEEPYDOG_WAKE_DEEP_TIMER:
  case SLEEPYDOG_WAKE_DEEP_PIN:
    break;
  }
  if (sleptMs == SLEEPYDOG_SLEPT_UNKNOWN) {
    sink = 2;
  }

  uint32_t words[SLEEPYDOG_PERSIST_WORDS] = {1, 2};
  bool ok = Watchdog.persist(words, SLEEPYDOG_PERSIST_WORDS, true);
  ok &= Watchdog.persist(words, 1, false);
  ok &= Watchdog.radioIsOn();

  // watchdog API on RP
  int ms = Watchdog.enable(4000);
  Watchdog.reset();
  Watchdog.disable();
  ms += Watchdog.sleep(10);
  ms += Watchdog.sleep();

#if defined(PICO_RP2350)
  // legacy RP2350 API
  Watchdog.setWakeCb(onWake);
  Watchdog.goToSleepUntil(1000);
  Watchdog.goToSleepUntil(1000, true);
  Watchdog.goToSleepUntilPin(2, true, false);
  Watchdog.resumeFromSleep();
  long d = Watchdog.getSleepDuration();
  ms += (int)d;
#endif

  sink = rc + (int)caps + why + (int)ok + ms;
}

void setup() {
  if (runApi) {
    apiTest();
  }
}

void loop() {}
