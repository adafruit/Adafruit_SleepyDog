// Be careful to use a platform-specific conditional include to only make the
// code visible for the appropriate platform.  Arduino will try to compile and
// link all .cpp files regardless of platform.
#if defined(ARDUINO_ARCH_SAMD)

#include <sam.h>

#include "WatchdogSAMD.h"

int WatchdogSAMD::enable(int maxPeriodMS) {
    // Enable the watchdog with a period up to the specified max period in
    // milliseconds.

    // Review the watchdog section from the SAMD21 datasheet section 17:
    //   http://www.atmel.com/images/atmel-42181-sam-d21_datasheet.pdf

    // Make sure watchdog and its associated clock is initialized only once.
    if (!_initialized) {
      _initialize_wdt();
    }

    // First disable the watchdog so its registers can be changed.
    WDT->CTRL.reg &= ~WDT_CTRL_ENABLE;
    while (WDT->STATUS.bit.SYNCBUSY);  // Syncronize write to CTRL reg.

    // Calculate the closest watchdog period to the desired period.
    int actualMS;
    uint8_t period;
    if ((maxPeriodMS >= 16384) || (maxPeriodMS == 0)) {
      actualMS = 16384;
      period = 0xB;
    }
    else if (maxPeriodMS >= 8192) {
      actualMS = 8192;
      period = 0xA;
    }
    else if (maxPeriodMS >= 4096) {
      actualMS = 4096;
      period = 0x9;
    }
    else if (maxPeriodMS >= 2048) {
      actualMS = 2048;
      period = 0x8;
    }
    else if (maxPeriodMS >= 1024) {
      actualMS = 1024;
      period = 0x7;
    }
    else if (maxPeriodMS >= 512) {
      actualMS = 512;
      period = 0x6;
    }
    else if (maxPeriodMS >= 256) {
      actualMS = 256;
      period = 0x5;
    }
    else if (maxPeriodMS >= 128) {
      actualMS = 128;
      period = 0x4;
    }
    else if (maxPeriodMS >= 64) {
      actualMS = 64;
      period = 0x3;
    }
    else if (maxPeriodMS >= 32) {
      actualMS = 32;
      period = 0x2;
    }
    else if (maxPeriodMS >= 16) {
      actualMS = 16;
      period = 0x1;
    }
    else {
      actualMS = 8;
      period = 0x0;
    }

    // Set watchdog period.
    WDT->CONFIG.reg = WDT_CONFIG_PER(period);
    while (WDT->STATUS.bit.SYNCBUSY);  // Syncronize write to CONFIG reg.

    // Disable early warning interrupt.
    WDT->INTENCLR.reg |= WDT_INTENCLR_EW;

    // Enable the watchdog.
    WDT->CTRL.reg |= WDT_CTRL_ENABLE;
    while (WDT->STATUS.bit.SYNCBUSY);  // Syncronize write to CTRL reg.

    return actualMS;
}

void WatchdogSAMD::reset() {
    // Write the watchdog clear key value (0xA5) to the watchdog clear register
    // to clear the watchdog timer and reset it.
    WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
    while (WDT->STATUS.bit.SYNCBUSY);  // Syncronize write to CLEAR reg.
}

void WatchdogSAMD::disable() {
  // Disable the watchdog.
  WDT->CTRL.reg &= ~WDT_CTRL_ENABLE;
  while (WDT->STATUS.bit.SYNCBUSY);  // Syncronize write to CTRL reg.
}

int WatchdogSAMD::sleep(int maxPeriodMS) {
    // Sleep code TBD!  Needs analysis of the SAMD21 datasheet.
    return 0;
}

void WatchdogSAMD::_initialize_wdt() {
    // Do one-time initialization of the watchdog timer.

    // Setup GCLK for the watchdog using:
    // - Generic clock generator 2 as the source for the watchdog clock
    // - Low power 32khz internal oscillator as the source for generic clock
    //   generator 2.
    // - Generic clock generator 2 divisor to 32 so it ticks roughly once a
    //   millisecond.

    // Set generic clock generator 2 divisor to 4 so the clock divisor is 32.
    // From the datasheet the clock divisor is calculated as:
    //   2^(divisor register value + 1)
    // A 32khz clock with a divisor of 32 will then generate a 1ms clock period.
    GCLK->GENDIV.reg = GCLK_GENDIV_ID(2) | GCLK_GENDIV_DIV(4);
    // Now enable clock generator 2 using the low power 32khz oscillator and the
    // clock divisor set above.
    GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(2) |
                        GCLK_GENCTRL_GENEN |
                        GCLK_GENCTRL_SRC_OSCULP32K |
                        GCLK_GENCTRL_DIVSEL;
    while (GCLK->STATUS.bit.SYNCBUSY);  // Syncronize write to GENCTRL reg.
    // Turn on the WDT clock using clock generator 2 as the source.
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_WDT |
                        GCLK_CLKCTRL_CLKEN |
                        GCLK_CLKCTRL_GEN_GCLK2;
}

#endif
