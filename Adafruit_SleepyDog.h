#ifndef ADAFRUIT_SLEEPYDOG_H
#define ADAFRUIT_SLEEPYDOG_H

// Platform-specific code goes below.  Each #ifdef should check for the presence
// of their platform and pull in the appropriate watchdog implementation type,
// then typedef it to WatchdogType so the .cpp file can create a global instance.
#if defined(ARDUINO_ARCH_AVR) || defined(__AVR__)
  #include "utility/WatchdogAVR.h"
  typedef WatchdogAVR WatchdogType;
#else
  #error Unsupported platform for the Adafruit Watchdog library!
#endif

extern WatchdogType Watchdog;

#endif