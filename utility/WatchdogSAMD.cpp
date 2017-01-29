// Requires Adafruit_ASFcore library!

// Be careful to use a platform-specific conditional include to only make the
// code visible for the appropriate platform.  Arduino will try to compile and
// link all .cpp files regardless of platform.
#if defined(ARDUINO_ARCH_SAMD)

#include <sam.h>
#include <Adafruit_ASFcore.h>
#include <power.h>
#include "WatchdogSAMD.h"

int WatchdogSAMD::enable(int maxPeriodMS, bool isForSleep) {
    // Enable the watchdog with a period up to the specified max period in
    // milliseconds.

    // Review the watchdog section from the SAMD21 datasheet section 17:
    //   http://www.atmel.com/images/atmel-42181-sam-d21_datasheet.pdf

    int     cycles, actualMS;
    uint8_t bits;

    if(!_initialized) _initialize_wdt();

    WDT->CTRL.reg = 0; // Disable watchdog for config
    while(WDT->STATUS.bit.SYNCBUSY);

    // You'll see some occasional conversion here compensating between
    // milliseconds (1000 Hz) and WDT clock cycles (~1024 Hz).  The low-
    // power oscillator used by the WDT ostensibly runs at 32,768 Hz with
    // a 1:32 prescale, thus 1024 Hz, though probably not super precise.

    if((maxPeriodMS >= 16000) || !maxPeriodMS) {
        cycles = 16384;
        bits   = 0xB;
    } else {
        cycles = (maxPeriodMS * 1024L + 500) / 1000; // ms -> WDT cycles
        if(cycles >= 8192) {
            cycles = 8192;
            bits   = 0xA;
        } else if(cycles >= 4096) {
            cycles = 4096;
            bits   = 0x9;
        } else if(cycles >= 2048) {
            cycles = 2048;
            bits   = 0x8;
        } else if(cycles >= 1024) {
            cycles = 1024;
            bits   = 0x7;
        } else if(cycles >= 512) {
            cycles = 512;
            bits   = 0x6;
        } else if(cycles >= 256) {
            cycles = 256;
            bits   = 0x5;
        } else if(cycles >= 128) {
            cycles = 128;
            bits   = 0x4;
        } else if(cycles >= 64) {
            cycles = 64;
            bits   = 0x3;
        } else if(cycles >= 32) {
            cycles = 32;
            bits   = 0x2;
        } else if(cycles >= 16) {
            cycles = 16;
            bits   = 0x1;
        } else {
            cycles = 8;
            bits   = 0x0;
        }
    }

    // Watchdog timer on SAMD is a slightly different animal than on AVR.
    // On AVR, the WTD timeout is configured in one register and then an
    // interrupt can optionally be enabled to handle the timeout in code
    // (as in waking from sleep) vs resetting the chip.  Easy.
    // On SAMD, when the WDT fires, that's it, the chip's getting reset.
    // Instead, it has an "early warning interrupt" with a different set
    // interval prior to the reset.  For equivalent behavior to the AVR
    // library, this requires a slightly different configuration depending
    // whether we're coming from the sleep() function (which needs the
    // interrupt), or just enable() (no interrupt, we want the chip reset
    // unless the WDT is cleared first).  In the sleep case, 'windowed'
    // mode is used in order to allow access to the longest available
    // sleep interval (about 16 sec); the WDT 'period' (when a reset
    // occurs) follows this and is always just set to the max, since the
    // interrupt will trigger first.  In the enable case, windowed mode
    // is not used, the WDT period is set and that's that.
    // The 'isForSleep' argument determines which behavior is used;
    // this isn't present in the AVR code, just here.  It defaults to
    // 'false' so existing Arduino code works as normal, while the sleep()
    // function (later in this file) explicitly passes 'true' to get the
    // alternate behavior.

    if(isForSleep) {
        WDT->INTENSET.bit.EW   = 1;      // Enable early warning interrupt
        WDT->CONFIG.bit.PER    = 0xB;    // Period = max
        WDT->CONFIG.bit.WINDOW = bits;   // Set time of interrupt
        WDT->CTRL.bit.WEN      = 1;      // Enable window mode
        while(WDT->STATUS.bit.SYNCBUSY); // Sync CTRL write
    } else {
        WDT->INTENCLR.bit.EW   = 1;      // Disable early warning interrupt
        WDT->CONFIG.bit.PER    = bits;   // Set period for chip reset
        WDT->CTRL.bit.WEN      = 0;      // Disable window mode
        while(WDT->STATUS.bit.SYNCBUSY); // Sync CTRL write
    }

    actualMS = (cycles * 1000L + 512) / 1024; // WDT cycles -> ms

    reset();                  // Clear watchdog interval
    WDT->CTRL.bit.ENABLE = 1; // Start watchdog now!
    while(WDT->STATUS.bit.SYNCBUSY);

    return actualMS;
}

void WatchdogSAMD::reset() {
    // Write the watchdog clear key value (0xA5) to the watchdog
    // clear register to clear the watchdog timer and reset it.
    WDT->CLEAR.reg = WDT_CLEAR_CLEAR_KEY;
    while(WDT->STATUS.bit.SYNCBUSY);
}

void WatchdogSAMD::disable() {
    WDT->CTRL.bit.ENABLE = 0;
    while(WDT->STATUS.bit.SYNCBUSY);
}

void WDT_Handler(void) {
    // ISR for watchdog early warning, DO NOT RENAME!
    WDT->CTRL.bit.ENABLE = 0;        // Disable watchdog
    while(WDT->STATUS.bit.SYNCBUSY); // Sync CTRL write
    WDT->INTFLAG.bit.EW  = 1;        // Clear interrupt flag
}

int WatchdogSAMD::sleep(int maxPeriodMS) {

    int actualPeriodMS = enable(maxPeriodMS, true); // true = for sleep

    system_set_sleepmode(SYSTEM_SLEEPMODE_STANDBY); // Deepest sleep
    system_sleep();
    // Code resumes here on wake (WDT early warning interrupt)

    return actualPeriodMS;
}

void WatchdogSAMD::_initialize_wdt() {
    // One-time initialization of watchdog timer.
    // Insights from rickrlh and rbrucemtl in Arduino forum!

    // Generic clock generator 2, divisor = 32 (2^(DIV+1))
    GCLK->GENDIV.reg = GCLK_GENDIV_ID(2) | GCLK_GENDIV_DIV(4);
    // Enable clock generator 2 using low-power 32KHz oscillator.
    // With /32 divisor above, this yields 1024Hz(ish) clock.
    GCLK->GENCTRL.reg = GCLK_GENCTRL_ID(2) |
                        GCLK_GENCTRL_GENEN |
                        GCLK_GENCTRL_SRC_OSCULP32K |
                        GCLK_GENCTRL_DIVSEL;
    while(GCLK->STATUS.bit.SYNCBUSY);
    // WDT clock = clock gen 2
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_WDT |
                        GCLK_CLKCTRL_CLKEN |
                        GCLK_CLKCTRL_GEN_GCLK2;

    // Enable WDT early-warning interrupt
    NVIC_DisableIRQ(WDT_IRQn);
    NVIC_ClearPendingIRQ(WDT_IRQn);
    NVIC_SetPriority(WDT_IRQn, 0); // Top priority
    NVIC_EnableIRQ(WDT_IRQn);

    _initialized = true;
}

#endif
