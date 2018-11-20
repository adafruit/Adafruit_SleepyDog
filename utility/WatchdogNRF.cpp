#if defined(NRF52832_XXAA) || defined(NRF52840_XXAA)

#include "Arduino.h"
#include "WatchdogNRF.h"
#include "nrf_wdt.h"

WatchdogNRF::WatchdogNRF()
{
  _wdto = -1;
}

int WatchdogNRF::enable(int maxPeriodMS)
{
  if (maxPeriodMS < 0) return 0;

  // cannot change wdt config register once it is started
  // return previous configured timeout
  if ( nrf_wdt_started() ) return _wdto;

  // WDT run when CPU is sleep
  nrf_wdt_behaviour_set(NRF_WDT_BEHAVIOUR_RUN_SLEEP);
  nrf_wdt_reload_value_set((maxPeriodMS * 32768) / 1000);

  // use channel 0
  nrf_wdt_reload_request_enable(NRF_WDT_RR0);

  // Start WDT
  // After started CRV, RREN and CONFIG is blocked
  // There is no way to stop/disable watchdog using source code
  // It can only be reset by WDT timeout, Pin reset, Power reset
  nrf_wdt_task_trigger(NRF_WDT_TASK_START);

  _wdto = maxPeriodMS;

  return maxPeriodMS;
}


void WatchdogNRF::reset()
{
  nrf_wdt_reload_request_set(NRF_WDT_RR0);
}

void WatchdogNRF::disable()
{
  // There is no way to stop/disable watchdog using source code
}

int WatchdogNRF::sleep(int maxPeriodMS)
{
  if (maxPeriodMS < 0) return 0;

  // Mimic AVR to use 8 seconds.
  if ( maxPeriodMS == 0 ) maxPeriodMS = 8000;

#ifdef ARDUINO_NRF52_ADAFRUIT
  // Bluefruit freeRTOS tickless implementation will
  // automatically put CPU into low power mode with delay()
  delay(maxPeriodMS);
#endif

  return maxPeriodMS;
}

#endif
