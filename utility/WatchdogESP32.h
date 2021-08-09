#ifndef WATCHDOGESP32_H_
#define WATCHDOGESP32_H_

class WatchdogESP32 {
public:
  WatchdogESP32() : _wdto(-1) {};

  // Enable the watchdog timer to reset the machine after a period of time
  // without any calls to reset().  The passed in period (in milliseconds) is
  // just a suggestion and a lower value might be picked if the hardware does
  // not support the exact desired value.
  //
  // The actual period (in milliseconds) before a watchdog timer reset is
  // returned.
  int enable(int maxPeriodMS = 0);

  // Reset or 'kick' the watchdog timer to prevent a reset of the device.
  void reset();

  // Completely disable the watchdog timer.
  void disable();

  // Enter ESP32 Deep-sleep mode
  int sleep(int maxPeriodMS = 0);

private:
  int _wdto;
};

#endif // WATCHDOGESP32_H