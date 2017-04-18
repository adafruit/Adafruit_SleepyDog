# Adafruit SleepyDog Arduino Library

Arduino library to use the watchdog timer for system reset and low power sleep.

Currently supports the following hardware:
*  Arduino Uno or other ATmega328P-based boards.
*  Arduino Mega or other ATmega2560- or 1280-based boards.
*  Arduino Zero, Adafruit Feather M0 (ATSAMD21). Requires Adafruit_ASFcore library -- install using Arduino Library Manager.
*  Arduino Leonardo or other 32u4-based boards (e.g. Adafruit Feather) WITH CAVEAT: USB Serial connection is clobbered on sleep; if sketch does not require Serial comms, this is not a concern. The example sketches all print to Serial and appear frozen, but the logic does otherwise continue to run.
*  Partial support for Teensy 3.X and LC (watchdog, no sleep).

Adafruit Trinket and other boards using ATtiny MCUs are NOT supported.
