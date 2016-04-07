// Adafruit Watchdog Library Sleep Example
//
// Simple example of how to do low power sleep with the watchdog timer.
//
// Author: Tony DiCola

#include <Adafruit_SleepyDog.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) ; // wait for Arduino Serial Monitor (native USB boards)
  Serial.println("Adafruit Watchdog Library Sleep Demo!");
  Serial.println();
}

void loop() {
  Serial.println("Going to sleep in one second...");
  delay(1000);
  
  // To enter low power sleep mode call Watchdog.sleep() like below
  // and the watchdog will allow low power sleep for as long as possible.
  // The actual amount of time spent in sleep will be returned (in 
  // milliseconds).
  int sleepMS = Watchdog.sleep();

  // Alternatively you can provide a millisecond value to specify
  // how long you'd like the chip to sleep, but the hardware only
  // supports a limited range of values so the actual sleep time might
  // be smaller.  The time spent in sleep will be returned (in
  // milliseconds).
  // int sleepMS = Watchdog.sleep(1000);  // Sleep for up to 1 second.

  Serial.print("I'm awake now!  I slept for ");
  Serial.print(sleepMS, DEC);
  Serial.println(" milliseconds.");
  Serial.println();
}
